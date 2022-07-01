/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/audio_codecs/audio_format.h"

#include <utility>

#include "absl/strings/match.h"
#include "api/array_view.h"
#include "rtc_base/bit_buffer.h"
#include "rtc_base/buffer.h"
#include "rtc_base/logging.h"

namespace webrtc {

const char kCPresent[] = "cpresent";
const char kConfig[] = "config";

SdpAudioFormat::SdpAudioFormat(const SdpAudioFormat&) = default;
SdpAudioFormat::SdpAudioFormat(SdpAudioFormat&&) = default;

SdpAudioFormat::SdpAudioFormat(absl::string_view name,
                               int clockrate_hz,
                               size_t num_channels)
    : name(name), clockrate_hz(clockrate_hz), num_channels(num_channels) {}

SdpAudioFormat::SdpAudioFormat(absl::string_view name,
                               int clockrate_hz,
                               size_t num_channels,
                               const Parameters& param)
    : name(name),
      clockrate_hz(clockrate_hz),
      num_channels(num_channels),
      parameters(param) {}

SdpAudioFormat::SdpAudioFormat(absl::string_view name,
                               int clockrate_hz,
                               size_t num_channels,
                               Parameters&& param)
    : name(name),
      clockrate_hz(clockrate_hz),
      num_channels(num_channels),
      parameters(std::move(param)) {}

bool SdpAudioFormat::Matches(const SdpAudioFormat& o) const {
  return absl::EqualsIgnoreCase(name, o.name) &&
         clockrate_hz == o.clockrate_hz && num_channels == o.num_channels;
}

bool SdpAudioFormat::IsCPresented() const {
  bool presented = false;

  for (auto& elem : parameters) {
    if (absl::EqualsIgnoreCase(elem.first, kCPresent) && 
        absl::EqualsIgnoreCase(elem.second, "1")) {
      presented = true;
    }
  }

  return presented;
}


bool SdpAudioFormat::GetInfoFromConfig(uint32_t& channels, uint32_t& sample_rate,
                                       bool& sbr_enabled, bool& ps_enabled,
                                       rtc::BufferT<uint8_t>& extra_data) const {
  bool is_ok = false;
  for (auto& param : parameters) {
    if (absl::EqualsIgnoreCase(param.first, kConfig)) {
      size_t cfg_str_len  = param.second.length();
      size_t cfg_data_len = cfg_str_len / 2;
      if (cfg_str_len <= 0 || 0 != cfg_str_len % 2) {
        RTC_LOG(LS_ERROR) << "SMC config string length(" << cfg_str_len <<") issue! config:" << param.second;
        return false;
      }

      rtc::BufferT<uint8_t> cfg_data(cfg_data_len, true);
      for (size_t i = 0; i < cfg_data_len; i++) {
        std::string num = param.second.substr(i * 2, 2);
        cfg_data.data()[i] = std::stoi(num, 0, 16);
      }

      is_ok = GetInfoFromConfigInternal(cfg_data.data(), cfg_data_len,
                                        channels, sample_rate,
                                        sbr_enabled, ps_enabled);

      //use the sdp's param as main param:
      sample_rate = sample_rate * (sbr_enabled ? 2 : 1);
      channels    = channels * (ps_enabled ? 2 : 1);

      extra_data.Clear();
      extra_data.SetData(cfg_data.data(), cfg_data.size());

      break;
    }
  }

  return is_ok;
}

const int aac_frequency_list[] = {
  96000,88200,64000,48000,44100,
  32000,24000,22050,16000,12000,
  11025,8000, 7350
};

bool SdpAudioFormat::GetInfoFromConfigInternal(uint8_t* data, size_t len,
                                               uint32_t& channels, uint32_t& sample_rate,
                                               bool& sbr_enabled, bool& ps_enabled) const {
  rtc::BitBuffer bb(data, len);  
  std::string desc;
  size_t ch = 0;
  size_t sr = 0;
  bool sbr = false;
  bool ps = false;

  desc = "audioMuxVersion";
  uint32_t audio_mux_version = 0;
  if (!bb.ReadBits(&audio_mux_version, 1)) {
    RTC_LOG(LS_ERROR) << "[AAC][LATM][SDPAF]Parse SMC bb.ReadBits [" << desc << "] err";
    return false;
  }
  if (0 != audio_mux_version) {
    RTC_LOG(LS_ERROR) << "[AAC][LATM][SDPAF]Parse SMC Validation err audio_mux_version(" << audio_mux_version << ") !=0";
    //if support(1 == audio_mux_version), do something more on audio_mux_versionA
    return false;
  }


  desc = "allStreamsSameTimeFraming";
  uint32_t all_stream_same_time_framing = 0;
  if (! bb.ReadBits(&all_stream_same_time_framing, 1)) {
    RTC_LOG(LS_ERROR) << "[AAC][LATM][SDPAF]Parse SMC bb.ReadBits [" << desc << "] err";
    return false; //no more value check
  }

  desc = "numSubFrames";
  uint32_t num_sub_frames;
  if (!bb.ReadBits(&num_sub_frames, 6)) {
    RTC_LOG(LS_ERROR) << "[AAC][LATM][SDPAF]Parse SMC bb.ReadBits [" << desc << "] err";
    return false;
  }
  if (0 != num_sub_frames) {
    RTC_LOG(LS_ERROR) << "[AAC][LATM][SDPAF]Parse SMC Validation err num_sub_frames(" << num_sub_frames << ")!=0";
    return false;
  }


  desc = "numProgram";
  uint32_t num_program;
  if (!bb.ReadBits(&num_program, 4)) {
    return false;
  }
  if (0 != num_program) {
    RTC_LOG(LS_ERROR) << "[AAC][LATM][SDPAF]Parse SMC Validation err num_program(" << num_program << ")!=0";
    return false;
  }

  desc = "numLayer";
  uint32_t num_layer;
  if (!bb.ReadBits(&num_layer, 3)) {
    RTC_LOG(LS_ERROR) << "[AAC][LATM][SDPAF]Parse SMC bb.ReadBits [" << desc << "] err";
    return false;
  }
  if (0 != num_layer) {
    RTC_LOG(LS_ERROR) << "[AAC][LATM][SDPAF]Parse SMC Validation err num_layer(" << num_layer << ")!=0";
    return false;
  }

  //desc = "useSameConfig";
  //uint32_t use_same_config;
  ////no need to valid, just read
  //if (!reader.Read(1, use_same_config, desc)) {
  //  return false;
  //}

  //ASC field:
  desc = "asc_audioObjectType";
  uint32_t asc_auido_object_type;
  if (!bb.ReadBits(&asc_auido_object_type, 5)) {
    RTC_LOG(LS_ERROR) << "[AAC][LATM][SDPAF]Parse SMC bb.ReadBits [" << desc << "] err";
    return false;
  }
  if (asc_auido_object_type >= 31) {
    RTC_LOG(LS_ERROR) << "[AAC][LATM]Parse SMC Validation err asc_auido_object_type(" << asc_auido_object_type << ") >= 31";
    return false;
  }

  if (5 == asc_auido_object_type || 29 == asc_auido_object_type) {
    sbr = true;
  }
  if (29 == asc_auido_object_type) {
    ps = true;
  }

  desc = "asc_samplingFrequencyIndex";
  uint32_t sr_idx;
  if (!bb.ReadBits(&sr_idx, 4)) {
    RTC_LOG(LS_ERROR) << "[AAC][LATM][SDPAF]Parse SMC bb.ReadBits [" << desc << "] err";
    return false;
  }
  if (sr_idx >= 0xC) {
    RTC_LOG(LS_ERROR) << "[AAC][LATM][SDPAF]Parse SMC Validation err on asc_samplingFrequencyIndex(" << sr_idx << ") >= 0xC";
    return false;
  }
  sr = aac_frequency_list[sr_idx];

  desc = "asc_channelConfiguration";
  uint32_t channel_cfg;
  if (!bb.ReadBits(&channel_cfg, 4)) {
    RTC_LOG(LS_ERROR) << "[AAC][LATM][SDPAF]Parse SMC bb.ReadBits [" << desc << "] err";
    return false;
  }
  if (2 != channel_cfg && 1 != channel_cfg) {
    RTC_LOG(LS_ERROR) << "[AAC][LATM][SDPAF]Parse SMC Validation err on asc_channelConfiguration(" << channel_cfg << ") != 1 or 2";
    return false;
  }
  ch = channel_cfg;

  sample_rate = sr;
  channels = ch;
  sbr_enabled = sbr;
  ps_enabled = ps;

  if (!param_logged_) {
    RTC_LOG(LS_INFO) << "[AAC][LATM]SMC check by SdpAudioFormat -- "
                    << "audio_mux_version(" << audio_mux_version << "), "
                    << "all_stream_same_time_framing(" << all_stream_same_time_framing << "), "
                    << "num_sub_frames(" << num_sub_frames << "), "
                    << "num_program(" << num_program << "), "
                    << "num_layer(" << num_layer << "), "
                    << "asc_auido_object_type(" << asc_auido_object_type << "), "
                    << "sbr(" << sbr << "), "
                    << "ps(" << ps << "),"
                    << "sr_idx(" << sr_idx << ")-" << sr << "kHz, "
                    << "channel_cfg(" << channel_cfg << ")-" << ch << "ch(s)";

    bool& dummy = const_cast<bool&>(param_logged_);
    dummy = true;
  }

  return true;
}

SdpAudioFormat::~SdpAudioFormat() = default;
SdpAudioFormat& SdpAudioFormat::operator=(const SdpAudioFormat&) = default;
SdpAudioFormat& SdpAudioFormat::operator=(SdpAudioFormat&&) = default;

bool operator==(const SdpAudioFormat& a, const SdpAudioFormat& b) {
  return absl::EqualsIgnoreCase(a.name, b.name) &&
         a.clockrate_hz == b.clockrate_hz && a.num_channels == b.num_channels &&
         a.parameters == b.parameters;
}

AudioCodecInfo::AudioCodecInfo(int sample_rate_hz,
                               size_t num_channels,
                               int bitrate_bps)
    : AudioCodecInfo(sample_rate_hz,
                     num_channels,
                     bitrate_bps,
                     bitrate_bps,
                     bitrate_bps) {}

AudioCodecInfo::AudioCodecInfo(int sample_rate_hz,
                               size_t num_channels,
                               int default_bitrate_bps,
                               int min_bitrate_bps,
                               int max_bitrate_bps)
    : sample_rate_hz(sample_rate_hz),
      num_channels(num_channels),
      default_bitrate_bps(default_bitrate_bps),
      min_bitrate_bps(min_bitrate_bps),
      max_bitrate_bps(max_bitrate_bps) {
  RTC_DCHECK_GT(sample_rate_hz, 0);
  RTC_DCHECK_GT(num_channels, 0);
  RTC_DCHECK_GE(min_bitrate_bps, 0);
  RTC_DCHECK_LE(min_bitrate_bps, default_bitrate_bps);
  RTC_DCHECK_GE(max_bitrate_bps, default_bitrate_bps);
}

}  // namespace webrtc
