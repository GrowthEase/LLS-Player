//
//  rtd_dec.c
//
//  Copyright © 2021 netease. All rights reserved.
//

#include <stdio.h>
#include <stdbool.h>
#include "libavformat/avformat.h"
#ifndef WIN32
#include "libavformat/url.h"
#endif
#include "libavutil/opt.h"
#include "libavutil/log.h"
#include "libavutil/time.h"
#include "libswresample/swresample.h"
#include "rtd_api.h"

#define RECV_STREAM_INFO_TIMEOUT 5000
#define STREAM_INFO_QUERY_INTERVAL 10
#define RTD_DEFAULT_AUDIO_FRAME_SAMPLES 1024

typedef struct RtdContext {
  AVClass* av_class;     // first element
  void* rtd_handler;  // instance created by open()
  int video_stream_index;
  int audio_stream_index;
  int need_drop_frame;
  pthread_mutex_t audio_decode_mutex;
  AVCodec* audio_decoder;
  AVCodecContext* audio_decoder_ctx;
  AVPacket audio_packet;
  AVFrame* audio_frame;
  bool audio_decoder_inited;
  int64_t channel_layout;
  enum AVSampleFormat sample_format;
  int sample_rate;
  int64_t out_channel_layout;
  enum AVSampleFormat out_sample_format;
  int out_sample_rate;
  struct SwrContext* audio_resample_ctx;
  pthread_mutex_t media_info_mutex;
  pthread_cond_t media_info_cond;
  bool media_info_received;
  struct RtdDemuxInfo stream_info;
  struct RtdApiFuncs* rtd_funcs;
  bool frame_dec_info_logged;
} RtdContext;

static int rtd_get_log_level() {
  int log_level = av_log_get_level();
  switch (log_level) {
  case AV_LOG_VERBOSE:
  case AV_LOG_DEBUG:
    return RTD_LOG_DEBUG;
  case AV_LOG_INFO:
    return RTD_LOG_INFO;
  case AV_LOG_WARNING:
    return RTD_LOG_WARN;
  case AV_LOG_ERROR:
    return RTD_LOG_ERROR;
  case AV_LOG_QUIET:
    return RTD_LOG_NONE;
  default:
    av_log(NULL, AV_LOG_ERROR, "invalid log level\n");
    return RTD_LOG_NONE;
  }
}

static int log_callback(AVFormatContext* s, int level, const char* log_msg) {
  int log_level = rtd_get_log_level();
  if (log_level <= level) {
    switch (level) {
    case RTD_LOG_DEBUG:
      av_log(NULL, AV_LOG_DEBUG, "%s\n", log_msg);
      break;
    case RTD_LOG_INFO:
      av_log(NULL, AV_LOG_INFO, "%s\n", log_msg);
      break;
    case RTD_LOG_WARN:
      av_log(NULL, AV_LOG_WARNING, "%s\n", log_msg);
      break;
    case RTD_LOG_ERROR:
      av_log(NULL, AV_LOG_ERROR, "%s\n", log_msg);
      break;
    case RTD_LOG_NONE:
      av_log(NULL, AV_LOG_QUIET, "%s\n", log_msg);
      break;
    default:
      break;
    }
  }

  return 0;
}

static int message_callback(AVFormatContext* s, int type, void* data, int data_size) {
  if (s->control_message_cb == NULL)
    return AVERROR(ENOSYS);

  return s->control_message_cb(s, type, data, (size_t)data_size);
}

static void media_info_callback(AVFormatContext* s, struct RtdDemuxInfo stream_info) {
  RtdContext* rtd = s->priv_data;
  rtd->stream_info = stream_info;
  rtd->media_info_received = true;
  pthread_mutex_lock(&rtd->media_info_mutex);
  pthread_cond_signal(&rtd->media_info_cond);
  pthread_mutex_unlock(&rtd->media_info_mutex);
}

static int rtd_audio_decoder_init(void* obj, RtdAudioDecoderInitParam* init_param) {
  AVFormatContext* s = obj;
  RtdContext* rtd = s->priv_data;
  if (!init_param) {
    av_log(NULL, AV_LOG_ERROR, "rtd_audio_decoder_init() err: init_param is null!\n");
    return -1;
  }
  av_log(NULL, AV_LOG_INFO, "rtd_audio_decoder_init channles:%d sample_rate:%d\n", init_param->num_channels, init_param->clockrate_hz);
  pthread_mutex_lock(&rtd->audio_decode_mutex);
  if (!rtd->audio_decoder_inited) {
    enum AVCodecID codec_id = init_param->use_latm ? AV_CODEC_ID_AAC_LATM : AV_CODEC_ID_AAC;
    rtd->audio_decoder = avcodec_find_decoder(codec_id);
    if (!rtd->audio_decoder) {
      av_log(NULL, AV_LOG_ERROR, "aac decoder find failed\n");
      pthread_mutex_unlock(&rtd->audio_decode_mutex);
      return -1;
    }
    av_init_packet(&rtd->audio_packet);
    rtd->audio_decoder_ctx = avcodec_alloc_context3(rtd->audio_decoder);
    if (!rtd->audio_decoder_ctx) {
      av_log(NULL, AV_LOG_ERROR, "aac decoder context alloc failed\n");
      pthread_mutex_unlock(&rtd->audio_decode_mutex);
      return AVERROR(ENOMEM);
    }

    rtd->audio_decoder_ctx->channel_layout = (init_param->num_channels == 1 ? AV_CH_LAYOUT_MONO : AV_CH_LAYOUT_STEREO);
    rtd->audio_decoder_ctx->codec_type = AVMEDIA_TYPE_AUDIO;
    rtd->audio_decoder_ctx->sample_rate = init_param->dec_hz;
    rtd->audio_decoder_ctx->channels = init_param->num_channels;
    rtd->audio_decoder_ctx->sample_fmt = AV_SAMPLE_FMT_S16;

    if (avcodec_open2(rtd->audio_decoder_ctx, rtd->audio_decoder, NULL) < 0) {
    av_log(NULL, AV_LOG_ERROR, "aac decoder create failed\n");
    pthread_mutex_unlock(&rtd->audio_decode_mutex);
    return -1;
    }

    rtd->audio_frame = av_frame_alloc();
    rtd->audio_decoder_inited = true;
  }
  pthread_mutex_unlock(&rtd->audio_decode_mutex);
  return 0;
}

static int rtd_audio_resample(RtdContext* rtd, uint8_t* buffer) {
  if (!rtd || !rtd->audio_frame) {
    return -1;
  }

  AVFrame* frame = rtd->audio_frame;
  int samples = frame->nb_samples;
  int64_t dec_channel_layout = 0;
  if (frame->channel_layout && frame->channels == av_get_channel_layout_nb_channels(frame->channel_layout)) {
    dec_channel_layout = frame->channel_layout;
  } else {
    dec_channel_layout = av_get_default_channel_layout(frame->channels);
  }

  enum AVSampleFormat dec_sample_format = frame->format;
  int dec_sample_rate = frame->sample_rate;

  if (dec_channel_layout != rtd->channel_layout ||
      dec_sample_format != rtd->sample_format ||
      dec_sample_rate != rtd->sample_rate) {
    swr_free(&rtd->audio_resample_ctx);
    rtd->audio_resample_ctx = swr_alloc_set_opts(NULL,
                                                 rtd->out_channel_layout,
                                                 rtd->out_sample_format,
                                                 rtd->out_sample_rate,
                                                 dec_channel_layout,
                                                 dec_sample_format,
                                                 dec_sample_rate,
                                                 0,
                                                 NULL);
    if (!rtd->audio_resample_ctx) {
      av_log(NULL, AV_LOG_ERROR, "rtd_audio_resample, Cannot create sample rate converter\n");
      return -1;
    }

    if (swr_init(rtd->audio_resample_ctx) < 0) {
      av_log(NULL, AV_LOG_ERROR, "rtd_audio_resample swr_init failed!\n");
      swr_free(&rtd->audio_resample_ctx);
      return -1;
    }

    rtd->channel_layout = dec_channel_layout;
    rtd->sample_format = dec_sample_format;
    rtd->sample_rate = dec_sample_rate;
  }

  if (rtd->audio_resample_ctx) {
    const uint8_t** in_buffer = (const uint8_t**)frame->data;
    uint8_t* out_buffer[8] = { NULL };
    out_buffer[0] = buffer;

    int dst_num_samples = av_rescale_rnd(swr_get_delay(rtd->audio_resample_ctx, rtd->out_sample_rate) + frame->nb_samples,
                                         rtd->out_sample_rate, rtd->out_sample_rate, AV_ROUND_UP);
    samples = swr_convert(rtd->audio_resample_ctx,
                          out_buffer,
                          dst_num_samples,
                          in_buffer,
                          frame->nb_samples);
  }

  return samples;
}

static int rtd_audio_decoder_decode(void* obj, RtdAudioDecodedInfo* info, int16_t* decoded) {
  if (!info || !info->encoded_data) {
    av_log(NULL, AV_LOG_INFO, "rtd_audio_decoder_decode falied info and info_data is null \n");
    return RTD_DEFAULT_AUDIO_FRAME_SAMPLES;
  }

  AVFormatContext* s = obj;
  RtdContext* rtd = s->priv_data;
  if (!rtd) {
    av_log(NULL, AV_LOG_INFO, "rtd_audio_decoder_decode falied rtd is null \n");
    return RTD_DEFAULT_AUDIO_FRAME_SAMPLES;
  }

  pthread_mutex_lock(&rtd->audio_decode_mutex);
  if (!rtd->audio_decoder_inited) {
    av_log(NULL, AV_LOG_INFO, "rtd_audio_decoder_decode falied audio_decoder_inited not init \n");
    pthread_mutex_unlock(&rtd->audio_decode_mutex);
    return RTD_DEFAULT_AUDIO_FRAME_SAMPLES;
  }

  if (!rtd->audio_decoder || !rtd->audio_decoder_ctx || !rtd->audio_frame) {
    av_log(NULL, AV_LOG_INFO, "rtd_audio_decoder_decode falied audio_decoder is null \n");
    pthread_mutex_unlock(&rtd->audio_decode_mutex);
    return RTD_DEFAULT_AUDIO_FRAME_SAMPLES;
  }

  rtd->audio_packet.data = info->encoded_data;
  rtd->audio_packet.size = info->encoded_size;
  if (avcodec_send_packet(rtd->audio_decoder_ctx, &rtd->audio_packet) == AVERROR(EAGAIN)) {
    av_log(NULL, AV_LOG_ERROR, "avcodec_send_packet failed\n");
    pthread_mutex_unlock(&rtd->audio_decode_mutex);
    return RTD_DEFAULT_AUDIO_FRAME_SAMPLES;
  }

  int ret = AVERROR(EAGAIN);
  do {
    ret = avcodec_receive_frame(rtd->audio_decoder_ctx, rtd->audio_frame);
    if (ret == AVERROR_EOF) {
      av_log(NULL, AV_LOG_ERROR, "audio decode failed\n");
      avcodec_flush_buffers(rtd->audio_decoder_ctx);
      pthread_mutex_unlock(&rtd->audio_decode_mutex);
      return RTD_DEFAULT_AUDIO_FRAME_SAMPLES;
    }

    if (ret >= 0) {
      break;
    }
  } while (ret != AVERROR(EAGAIN));

  if (ret == AVERROR(EAGAIN)) {
    av_log(NULL, AV_LOG_INFO, "rtd_audio_decoder_decode falied ret is EAGAIN \n");
    pthread_mutex_unlock(&rtd->audio_decode_mutex);
    return RTD_DEFAULT_AUDIO_FRAME_SAMPLES;
  }

  if (!rtd->frame_dec_info_logged && rtd->audio_frame) {
    AVCodecParameters* cparam = NULL;

    cparam = avcodec_parameters_alloc();
    ret = avcodec_parameters_from_context(cparam, rtd->audio_decoder_ctx);

    av_log(NULL, AV_LOG_INFO, "[AAC][RTD_DEC]frame dec info, ch:%d, sr:%d, smpl_per_frame:%d, "
                              "profile:%d, level:%d \n",
                              rtd->audio_frame->channels, rtd->audio_frame->sample_rate, 
                              rtd->audio_frame->nb_samples,
                              ((!ret) ? cparam->profile : -1), ((!ret) ? cparam->level : -1));

    if (cparam) {
      avcodec_parameters_free(&cparam);
      cparam = NULL;
    }

    rtd->frame_dec_info_logged = true;
    ret = 0;
  }

  rtd->out_channel_layout = info->dst_channels < 2 ? AV_CH_LAYOUT_MONO : AV_CH_LAYOUT_STEREO;
  int frame_samples = rtd->audio_frame->nb_samples;
  int resample_samples = rtd_audio_resample(rtd, (uint8_t*)decoded);
  if (resample_samples > 0) {
    frame_samples = resample_samples;
  }
  pthread_mutex_unlock(&rtd->audio_decode_mutex);
  return frame_samples;
}

static int rtd_audio_decoder_uninit(void* obj) {
  av_log(NULL, AV_LOG_INFO, "rtd_audio_decoder_uninit\n");
  AVFormatContext* s = obj;
  RtdContext* rtd = s->priv_data;
  if (!rtd) {
    return -1;
  }

  pthread_mutex_lock(&rtd->audio_decode_mutex);
  if (rtd->audio_decoder_ctx) {
    avcodec_close(rtd->audio_decoder_ctx);
    avcodec_free_context(&rtd->audio_decoder_ctx);
    rtd->audio_decoder_ctx = NULL;
  }

  if (rtd->audio_frame) {
    av_frame_free(&rtd->audio_frame);
    rtd->audio_frame = NULL;
  }

  if (rtd->audio_resample_ctx) {
    swr_free(&rtd->audio_resample_ctx);
    rtd->audio_resample_ctx = NULL;
  }

  av_packet_unref(&(rtd->audio_packet));

  rtd->frame_dec_info_logged = false;

  pthread_mutex_unlock(&(rtd->audio_decode_mutex));
  return 0;
}

static int initialize(RtdContext* rtd) {
  pthread_mutex_init(&rtd->audio_decode_mutex, NULL);
  pthread_mutex_init(&rtd->media_info_mutex, NULL);
  pthread_cond_init(&rtd->media_info_cond, NULL);
  rtd->media_info_received = false;
  rtd->audio_frame = NULL;
  rtd->audio_decoder = NULL;
  rtd->audio_decoder_ctx = NULL;
  rtd->audio_resample_ctx = NULL;
  rtd->channel_layout = 0;
  rtd->sample_format = AV_SAMPLE_FMT_NONE;
  rtd->sample_rate = 0;
  rtd->out_channel_layout = AV_CH_LAYOUT_STEREO;
  rtd->out_sample_format = AV_SAMPLE_FMT_S16;
  rtd->out_sample_rate = 48000;
  rtd->audio_decoder_inited = false;
  return 0;
}

static void set_stream_pts_info(AVStream *s, int pts_wrap_bits,
                                unsigned int pts_num, unsigned int pts_den) {
  AVRational new_tb;
  av_reduce(&new_tb.num, &new_tb.den, pts_num, pts_den, INT_MAX);
  if (new_tb.num <= 0 || new_tb.den <= 0) {
    return;
  }
  s->time_base     = new_tb;
  s->pts_wrap_bits = pts_wrap_bits;
}

static int rtd_stream_info_init(AVFormatContext *s){
  if (!s)
    return AVERROR(EINVAL);

  s->flags |= AVFMT_FLAG_GENPTS;
  s->fps_probe_size = 0;
  RtdContext* rtd = s->priv_data;
  rtd->audio_stream_index = -1;
  rtd->video_stream_index = -1;

  if (rtd->stream_info.audio_enabled) {
    int stream_index = s->nb_streams;
    AVStream *stream = avformat_new_stream(s, NULL);
    if (!stream) {
      av_log(s, AV_LOG_ERROR, "alloc audio stream failed!\n");
      return AVERROR(ENOMEM);
    }

    stream->codecpar->codec_type = AVMEDIA_TYPE_AUDIO;
    set_stream_pts_info(stream, 64, 1, 1000);
    s->streams[stream_index]                        = stream;
    s->streams[stream_index]->need_parsing          = AVSTREAM_PARSE_NONE;
    s->streams[stream_index]->codecpar->codec_id    = s->audio_codec_id = AV_CODEC_ID_PCM_S16LE;
    s->streams[stream_index]->codecpar->channels    = rtd->stream_info.audio_channels;
    s->streams[stream_index]->codecpar->sample_rate = rtd->stream_info.audio_sample_rate;
    s->streams[stream_index]->codecpar->format      = AV_SAMPLE_FMT_S16;
    rtd->audio_stream_index = stream_index;
  }

  if (rtd->stream_info.video_enabled) {
    int stream_index = s->nb_streams;
    AVStream *stream = avformat_new_stream(s, NULL);
    if (!stream) {
      av_log(s, AV_LOG_ERROR, "alloc video stream failed!\n");
      return AVERROR(ENOMEM);
    }
    stream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
    set_stream_pts_info(stream, 64, 1, 1000);
    s->streams[stream_index] = stream;
    s->streams[stream_index]->codecpar->codec_id = s->video_codec_id = AV_CODEC_ID_H264;
    s->streams[stream_index]->need_parsing       = AVSTREAM_PARSE_NONE;
    rtd->video_stream_index = stream_index;
  }
  s->probesize = 40*1024;
  s->max_analyze_duration = 0.2 * AV_TIME_BASE;
  return AVERROR(0);
}

static int rtd_read_close(AVFormatContext *s) {
  if (!s)
    return AVERROR(EINVAL);
  av_log(s,AV_LOG_INFO, "rtd_read_close in!\n");

  if (s->pb) {
    av_freep(s->pb);
  }
  RtdContext* rtd = s->priv_data;
  if (rtd->rtd_handler) {
    rtd->rtd_funcs->close(rtd->rtd_handler);
    rtd->rtd_handler = NULL;
  }

  memset(rtd, 0, sizeof(RtdContext));
  av_log(s, AV_LOG_INFO, "rtd_read_close out!\n");
  return AVERROR(0);
}

static int rtd_probe(AVProbeData *p){
  if (strstr(p->filename, "nertc://")) {
    return AVPROBE_SCORE_MAX;
  }
  return 0;
}

static int rtd_read_header(AVFormatContext *s){
  if (!s)
    return AVERROR(EINVAL);
  av_log(s,AV_LOG_INFO, "rtd_read_header in!\n");
  RtdContext* rtd = s->priv_data;
  initialize(rtd);
  int ret = AVERROR(0);

  rtd->rtd_funcs = GetRtdApiFuncs(0);
  if (!rtd->rtd_funcs) {
    av_log(s,AV_LOG_ERROR, "get rtd impl failed!\n");
    ret = AVERROR(EINVAL);
    goto out;
  }
  rtd->need_drop_frame = 0;

  if(rtd->rtd_handler){
    rtd->rtd_funcs->close(rtd->rtd_handler);
    rtd->rtd_handler = NULL;
  }

  RtdConf conf;
  memset(&conf, 0, sizeof(RtdConf));
  conf.log_level = rtd_get_log_level();
  conf.ff_ctx = s;
  conf.callbacks.msg_callback = message_callback;
  conf.callbacks.log_sink = log_callback;
  conf.callbacks.media_info_callback = media_info_callback;
  conf.callbacks.audio_decoder_init = rtd_audio_decoder_init;
  conf.callbacks.audio_decode = rtd_audio_decoder_decode;
  conf.callbacks.audio_decoder_uninit = rtd_audio_decoder_uninit;

  rtd->rtd_handler = rtd->rtd_funcs->create(conf);
  if (!rtd->rtd_handler) {
    av_log(s, AV_LOG_ERROR, "fail to create rtd instance!\n");
    ret = AVERROR(EIO);
    goto out;
  }

  int ret_open = rtd->rtd_funcs->open(rtd->rtd_handler, s->filename, "r");
  if (ret_open != RTD_ERROR_OPEN_SUCCESS) {
    av_log(s, AV_LOG_ERROR, "fail to open the link! error_code:%d\n", ret_open);
    ret = AVERROR(ret_open);
    goto out;
  }

  for (int i = 0; i < RECV_STREAM_INFO_TIMEOUT / STREAM_INFO_QUERY_INTERVAL; i++) {
#ifndef WIN32
    if (ff_check_interrupt(&s->interrupt_callback)) {
     av_log(s, AV_LOG_INFO, "user interrupted\n");
     ret = AVERROR_EXIT;
     goto out;
    }
#endif
    pthread_mutex_lock(&rtd->media_info_mutex);
    int64_t time = av_gettime() + STREAM_INFO_QUERY_INTERVAL*1000;
    struct timespec timeout = { .tv_sec = time / 1000000, .tv_nsec = (time % 1000000) * 1000 };
    if (pthread_cond_timedwait(&rtd->media_info_cond, &rtd->media_info_mutex, &timeout) == 0 ||
      rtd->media_info_received) {
      av_log(s, AV_LOG_INFO, "received media info\n");
      pthread_mutex_unlock(&rtd->media_info_mutex);
      break;
    }

    pthread_mutex_unlock(&rtd->media_info_mutex);
  }

  ret = rtd_stream_info_init(s);
  av_log(s, AV_LOG_INFO, "init stream info, ret is %d\n", ret);

out:
  if (ret != AVERROR(0))
    rtd_read_close(s);
  av_log(s, AV_LOG_INFO, "rtd_read_header out!\n");
  return ret;
}

static int rtd_read_packet(AVFormatContext *s, AVPacket *pkt) {
  if (!s)
    return AVERROR(EINVAL);

  RtdContext* rtd = s->priv_data;
  int ret = AVERROR(0);
#ifndef WIN32
  if (ff_check_interrupt(&s->interrupt_callback)) {
   av_log(s, AV_LOG_INFO, "user exit in read packet!\n");
   return AVERROR_EXIT;
  }
#endif
  struct RtdFrame *frame = NULL;
  if (rtd->rtd_funcs) {
    ret = rtd->rtd_funcs->read(&frame, rtd->rtd_handler);
    if (ret < 0) {
      av_log(s, AV_LOG_ERROR, "read frame failed!\n");
      return AVERROR(EIO);
    }
  } else {
    av_log(s, AV_LOG_ERROR, "read frame failed, rtd_funcs is nullptr!\n");
    return AVERROR(EIO);
  }

  if (!frame)
    return AVERROR(EAGAIN);

  if (frame->is_audio && rtd->audio_stream_index >= 0) { //audio frame
    pkt->stream_index = rtd->audio_stream_index;
  } else if (!frame->is_audio && rtd->video_stream_index >= 0) { //video frame
    if (frame->flag & 0x01) {
      pkt->flags |= AV_PKT_FLAG_KEY;
      rtd->need_drop_frame = 0;
    }

    if (rtd->need_drop_frame) {
      pkt->flags = 0;
      ret = AVERROR(EAGAIN);
      goto out;
    }
    pkt->stream_index = rtd->video_stream_index;
  } else {  //wrong frame
    ret = AVERROR(EAGAIN);
    goto out;
  }

  pkt->dts = frame->dts;
  pkt->pts = frame->pts;
  pkt->duration = frame->duration;
    
  AVBufferRef *buf = av_buffer_alloc(frame->size + AV_INPUT_BUFFER_PADDING_SIZE);
  if (!buf) {
    av_log(s, AV_LOG_ERROR, "alloc pkt buf out of memory!\n");
    ret = AVERROR(ENOMEM);
    goto out;
  }

  memcpy(buf->data, frame->buf, frame->size);
  pkt->buf = buf;
  pkt->data = buf->data;
  pkt->size = frame->size;
  ret = AVERROR(0);

out:
  rtd->rtd_funcs->free_frame(frame, rtd->rtd_handler);
  return ret;
}

#define OFFSET(x) offsetof(RtdContext, x)
#define DEC AV_OPT_FLAG_DECODING_PARAM
static const AVOption ff_rtd_options[] = {
  { NULL }
};

static const AVClass rtd_demuxer_class = {
  .class_name     = "rtd demuxer",
  .item_name      = av_default_item_name,
  .option         = ff_rtd_options,
  .version        = LIBAVUTIL_VERSION_INT,
};

AVInputFormat ff_rtd_demuxer = {
  .name               = "rtd", // rtd stream
  .long_name          = "rtd stream input",
  .priv_data_size     = sizeof(RtdContext),
  .read_probe         = rtd_probe,
  .read_header        = rtd_read_header,
  .read_packet        = rtd_read_packet,
  .read_close         = rtd_read_close,
  .flags              = AVFMT_NOFILE,
  .priv_class         = &rtd_demuxer_class,
  .extensions         = "rtd",
};
