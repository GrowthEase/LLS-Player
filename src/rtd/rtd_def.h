#ifndef RTD_DEF_H_
#define RTD_DEF_H_

#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

#define RTD_EXPORTS
#if defined(_WIN32)
#ifdef RTD_EXPORTS
#define RTD_API __declspec(dllexport)
#else
#define RTD_API __declspec(dllimport)
#endif
#else
#if defined(RTD_EXPORTS)
#define RTD_API __attribute__ ((visibility("default")))
#else
#define RTD_API
#endif
#endif

// error code
#define RTD_ERROR_BASE 10000
#define RTD_ERROR_OPEN_SUCCESS              (RTD_ERROR_BASE + 200)
#define RTD_ERROR_NULL_IS_PTR               (RTD_ERROR_BASE + 300)
#define RTD_ERROR_UNINITIALIZE              (RTD_ERROR_BASE + 301)
#define RTD_ERROR_CONNECTION_TIMEOUT        (RTD_ERROR_BASE + 302)
#define RTD_ERROR_PARAMS_ILLEGAL            (RTD_ERROR_BASE + 400)
#define RTD_ERROR_AUTH_FAILED               (RTD_ERROR_BASE + 403)
#define RTD_ERROR_STREAM_NOT_FOUND          (RTD_ERROR_BASE + 404)
#define RTD_ERRPR_APPKEY_ILLEGAL            (RTD_ERROR_BASE + 414)
#define RTD_ERROR_SERVER_CONNECTION_FAILED  (RTD_ERROR_BASE + 500)
#define RTD_ERROR_MEDIA_CONNECTION_FAILED   (RTD_ERROR_BASE + 600)
#define RTD_ERROR_FIRST_VIDEO_FRAME_TIMEOUT (RTD_ERROR_BASE + 601)
#define RTD_ERROR_MEDIA_STREAM_STOPPED      (RTD_ERROR_BASE + 602)

#define RTD_HEADER_LEN 1024

typedef enum RtdMessageType {
  RTD_MSG_OFFER_SDP,
  RTD_MSG_TRACE_ID,
} RtdMessageType;

typedef enum RtdLogLevel {
  RTD_LOG_DEBUG,
  RTD_LOG_INFO,
  RTD_LOG_WARN,
  RTD_LOG_ERROR,
  RTD_LOG_NONE,
} RtdLogLevel;

// structure to store subscribed stream info
// use command(..., "getStreamInfo", ...) to fetch
typedef struct RtdDemuxInfo {
  // audio part
  int audio_enabled;      // 1 - following audio info is valid; 0 - invalid
  int audio_channels;     // 1
  int audio_sample_rate;  // 48000

  // video part
  int video_enabled;      // 1 - following video info is valid; 0 - invalid
  int video_codec;        // 1 - h264  2 - hevc
  int video_width;
  int video_height;
  int video_profile;
  int video_level;

  int spspps_len;         // actual bytes used in spspps
  unsigned char spspps[RTD_HEADER_LEN]; // large enough
} RtdDemuxInfo;

typedef struct RtdFrame {
  void* buf;              // where frame data is stored
  int size;               // size of frame data in bytes
  int is_audio;           // 1 for audio frame, 0 for video frame
  uint64_t pts;           // presentation time stamp, in ms
  uint64_t dts;           // decoding time stamp, in ms
  int flag;               // for video frame (is_audio == 0)
                          // bit 0: key frame;
  int duration;           // in ms
} RtdFrame;

typedef struct RtdAudioDecodedInfo {
  uint8_t* encoded_data;
  int encoded_size;
  int dst_sample_rate;
  int dst_channels;
} RtdAudioDecodedInfo;

typedef struct RtdAudioDecoderInitParam {
  int num_channels;
  int dec_hz;
  int clockrate_hz;
  bool use_latm;
  bool sbr_enabled;
  bool ps_enabled;
  uint8_t* extra_data;
  int  extra_data_len;
} RtdAudioDecoderInitParam;

typedef void(*ExternalLogSink)(void* ctx, int level, const char* data);
typedef int(*MsgCallback)(void* ctx, int type, void* data, int data_size);
typedef void(*MediaInfoCallback)(void* ctx, struct RtdDemuxInfo stream_info);
typedef int(*ExternalAudioDecoderInit)(void* ctx, RtdAudioDecoderInitParam* init_param);
typedef int(*ExternalAudioDecode)(void* ctx, RtdAudioDecodedInfo* info, int16_t* decoded);
typedef int(*ExternalAudioDecoderUninit)(void* ctx);

typedef struct RtdCallbacks {
  ExternalLogSink log_sink; // log callback
  MsgCallback msg_callback; // message callback
  MediaInfoCallback media_info_callback; // media info callback
  ExternalAudioDecoderInit audio_decoder_init; // externl audio decoder init
  ExternalAudioDecode audio_decode;  // external audio decode
  ExternalAudioDecoderUninit audio_decoder_uninit; // external audio decoder uninit
} RtdCallbacks;

typedef struct RtdConf {
  RtdLogLevel log_level;
  void* ff_ctx;            // AVFormatContext
  RtdCallbacks callbacks;  // callbacks
} RtdConf;

#if defined(_WIN32)
#include <windows.h>
#include <time.h>
/* use light weight mutex/condition variable API for Windows Vista and later */
typedef SRWLOCK pthread_mutex_t;
typedef CONDITION_VARIABLE pthread_cond_t;
static inline int pthread_mutex_init(pthread_mutex_t* m, void* attr) {
  InitializeSRWLock(m);
  return 0;
}
static inline int pthread_mutex_destroy(pthread_mutex_t* m) {
  /* Unlocked SWR locks use no resources */
  return 0;
}
static inline int pthread_mutex_lock(pthread_mutex_t* m) {
  AcquireSRWLockExclusive(m);
  return 0;
}
static inline int pthread_mutex_unlock(pthread_mutex_t* m) {
  ReleaseSRWLockExclusive(m);
  return 0;
}

static inline int pthread_cond_init(pthread_cond_t* cond, const void* unused_attr) {
  InitializeConditionVariable(cond);
  return 0;
}

/* native condition variables do not destroy */
static inline int pthread_cond_destroy(pthread_cond_t* cond) {
  return 0;
}

static inline int pthread_cond_broadcast(pthread_cond_t* cond) {
  WakeAllConditionVariable(cond);
  return 0;
}

static inline int pthread_cond_wait(pthread_cond_t* cond, pthread_mutex_t* mutex) {
  SleepConditionVariableSRW(cond, mutex, INFINITE, 0);
  return 0;
}

static inline int pthread_cond_timedwait(pthread_cond_t* cond, pthread_mutex_t* mutex,
  const struct timespec* abstime) {
  int64_t abs_milli = abstime->tv_sec * 1000LL + abstime->tv_nsec / 1000000;
 // DWORD t = av_clip64(abs_milli - av_gettime() / 1000, 0, UINT32_MAX);

  FILETIME ft;
  int64_t tt;
  GetSystemTimeAsFileTime(&ft);
  tt = (int64_t)ft.dwHighDateTime << 32 | ft.dwLowDateTime;
  DWORD t = abs_milli - (tt / 10 - 11644473600000000) / 1000; /* Jan 1, 1601 */

  if (!SleepConditionVariableSRW(cond, mutex, t, 0)) {
    DWORD err = GetLastError();
    if (err == ERROR_TIMEOUT)
      return ETIMEDOUT;
    else
      return EINVAL;
  }
  return 0;
}

static inline int pthread_cond_signal(pthread_cond_t* cond) {
  WakeConditionVariable(cond);
  return 0;
}
#endif

#if defined(__cplusplus)
}
#endif

#endif // !RTD_DEF_H_

