#ifndef RTD_API_H_
#define RTD_API_H_

#include "rtd_def.h"

#if defined(__cplusplus)
extern "C" {
#endif

// Api functions to manipulate RTC streams
typedef struct RtdApiFuncs {
  int version; // used for validation, default 0.

  /* create rtd instance
   * conf: configure parameters
   * return value: instance of rtd. NULL if create failed
   */
  void* (*create)(RtdConf conf);

  /* open a stream specified by url
   * url:   stream url. Rtc stream supported for now
   * mode:  "r" for subscribe. publish is not implemented yet
   * return value:.10200 if open success
   */
  int (*open)(void* handle, const char* url, const char* mode);

  /* close the stream
   * handle: returned by open
   */
  void (*close)(void* handle);

  /* runtime command (e.g. get/set parameters)
   * @return 0 for success, negative value for error
   */
  int (*command)(void* handle, const char* cmd, void* arg);

  /* read one frame
   * caller need free the returned frame
   * return value: 1 for one frame read into '*frame'; 0 for try
   *               later; -1 for EOF; or other negative value for
   *               fatal error
   */
  int (*read)(struct RtdFrame** frame, void* handle);

  /* free RtdFrame of current stream
   * handle to the stream returned by open
   */
  void (*free_frame)(struct RtdFrame* frame, void* handle);
} RtdApiFuncs;

/* @brief Query Rtd Api functions
 * @param version    Specify compatible api version, default:0
 * @return Structure containing Api function pointers
 */
RTD_API const struct RtdApiFuncs* GetRtdApiFuncs(int version);

#if defined(__cplusplus)
}
#endif


#endif // !RTD_API_H_
