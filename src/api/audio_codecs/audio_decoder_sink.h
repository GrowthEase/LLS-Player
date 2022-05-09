#ifndef API_AUDIO_CODECS_AUDIO_DECODER_SINK_H_
#define API_AUDIO_CODECS_AUDIO_DECODER_SINK_H_

#include <stdint.h>
#include "rtc_base/buffer.h"

struct DecoderInitParam {
  int   num_channels;
  int   dec_hz;
  int   clockrate_hz;
  bool  use_latm;
  bool  sbr_enabled;
  bool  ps_enabled;

  rtc::BufferT<uint8_t> extra_data;
};

class AudioDecoderSink {
 public:
  virtual ~AudioDecoderSink() = default;
  virtual int AudioDecoderInit(struct DecoderInitParam& init_param) = 0;
  virtual int DecodeAudio(uint8_t* encoded, uint32_t encoded_len,
                          uint32_t sample_rate, uint32_t channels,
                          int16_t* decoded) = 0;
  virtual int AudioDecoderUninit() = 0;
};

#endif