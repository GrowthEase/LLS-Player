#ifndef RTD_INTERFACE_H_
#define RTD_INTERFACE_H_

namespace webrtc {
namespace rtd {

typedef enum RtdMediaType {
  RTD_AUDIO = 0,
  RTD_VIDEO,
} RtdMediaType;

typedef enum RtdVideoCodecType {
  RTD_H264 = 0,
  RTD_H265,
} RtdVideoCodecType;

typedef enum RtdAudioCodecType {
  RTD_OPUS = 0,
  RTD_AAC,
} RtdAudioCodecType;

typedef enum RtdFrameType {
  RTD_KEY_FRAME = 0,
  RTD_DELTA_FRAME,
} RtdFrameType;

typedef struct RtdAudioFrame {
  int16_t* data;
  size_t samples_per_channel;
  int sample_rate_hz;
  size_t num_channels;
  int64_t timestamp_ms;
  int64_t timestamp_rtp;
  RtdAudioCodecType codec_type;
} RtdAudioFrame;

typedef struct RtdVideoFrame {
  uint8_t* data;
  size_t size;
  int64_t play_timestamp_ms;
  int64_t timestamp_ms;
  int64_t timestamp_rtp;
  RtdVideoCodecType codec_type;
  RtdFrameType frame_type;
} RtdVideoFrame;

typedef enum RtdOpenFailReason {
  RTD_NONE_FAILED = 0,
  RTD_INITIALIZE_FAILED,
  RTD_SIGNALING_FAILED,
  RTD_ICE_FAILED,
  RTD_FIRST_FRAME_TIMEOUT,
} RtdOpenFailReason;

typedef enum RtdMediaConnStatus {
  RTD_MEDIA_CONN_NONE = 0,
  RTD_MEDIA_CONN_FAILED,
  RTD_MEDIA_CONN_SUCCESS,
} RtdMediaConnStatus;

} // namespace webrtc
} // namespace rtd

#endif // !RTD_INTERFACE_H_