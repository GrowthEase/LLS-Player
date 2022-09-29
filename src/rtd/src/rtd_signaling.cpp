#include "rtd_signaling.h"
#include "rtc_base/logging.h"
#include "third_party/jsoncpp/source/include/json/json.h"

#include "rtd_engine_interface.h"

namespace {

constexpr int kDefaultSignalingTimeoutMs = 5000; // ms
constexpr int kDefaultSignalingConnTimeoutMs = 2000; //ms
constexpr char kSignalingServerDomain[] = "http://wecan-api.netease.im/v1/live/play";
constexpr char kWhipSignalingServerDomain[] = "https://roomserver-greytest.netease.im/v1/whip/endpoint/play";
constexpr char kSdkVersion[] = "1.2.0";
constexpr char kTestAppkey[] = "9f9faeaa43f4593ace13b5bfa22c2bb4"; // for test, can change the value based on the actual situation

} // namespace

namespace webrtc {
namespace rtd {

RtdSignaling::RtdSignaling(const std::string& url)
    : url_(url),
      server_domain_(kSignalingServerDomain),
      whip_server_domain_(kWhipSignalingServerDomain),
      request_id_(""),
      cid_(""),
      uid_(""),
      timeout_ms_(kDefaultSignalingTimeoutMs) {
  RTC_LOG(LS_INFO) << "RtdSignaling::RtdSignaling().";
}

RtdSignaling::~RtdSignaling() {
  RTC_LOG(LS_INFO) << "RtdSignaling::~RtdSignaling().";
}

int RtdSignaling::ConnectAndWaitResponse(std::string& offer_sdp, std::string* answer_sdp) {
  RTC_LOG(LS_INFO) << "RtdSignaling::ConnectAndWaitResponse";
  http_.reset(new RtdHttp(server_domain_, timeout_ms_, kDefaultSignalingConnTimeoutMs));
  if (!http_) {
    RTC_LOG(LS_ERROR) << "RtdSignaling::ConnectAndWaitResponse signaling instance is nullptr.";
    return -1;
  }
  Json::StreamWriterBuilder writer_builder;
  writer_builder["commentStyle"] = "None";
  writer_builder["indentation"] = "";

  Json::Value root;
  root["version"] = 1;
  root["appkey"] = kTestAppkey;
  root["sdk_version"] = kSdkVersion;
  root["mode"] = "live";

  Json::Value pull_stream;
  pull_stream["url"] = url_;
  root["pull_stream"] = pull_stream;

  Json::Value jsep;
  jsep["type"] = "offer";
  jsep["sdp"] = offer_sdp;
  root["jsep"] = jsep;

  std::string request_str = Json::writeString(writer_builder, root);

  http_->AddHeader("Content-Type", "application/json");
  http_->AddHeader("RequestId", request_id_);
  http_->AddContent(true, "", request_str);
  RTC_LOG(LS_INFO) << "RtdSignaling::DoEasy send request";
  int curl_code = http_->DoEasy();
  if (curl_code != 0) {
    RTC_LOG(LS_ERROR) << "RtdSignaling::DoEasy failed. code:" << curl_code;
    return -1;
  }

  RTC_LOG(LS_INFO) << "RtdSignaling::DoEasy success. code:" << curl_code;
  Json::CharReaderBuilder reader_builder;
  std::unique_ptr<Json::CharReader> reader(reader_builder.newCharReader());
  std::string json_err;

  root.clear();
  std::string content = http_->GetContent();
  if (!reader->parse(content.c_str(), content.c_str() + content.length(), &root, &json_err) || !root.isObject()) {
    RTC_LOG(LS_ERROR) << "RtdSignaling Response Json parse failed:" << json_err.c_str() << " Content:" << content.c_str();
    return -1;
  }

  writer_builder["commentStyle"] = "All";
  writer_builder["indentation"] = "	";

  std::string trace_id = root["trace_id"].asString();
  RTC_LOG(LS_INFO) << "RtdSignaling Response trace_id:" << trace_id;
  std::string styled_content = Json::writeString(writer_builder, root);
  RTC_LOG(LS_INFO) << "RtdSignaling Response Answer SDP:" << styled_content;
  Json::Value json_pull_stream = root["pull_stream"];
  cid_ = json_pull_stream["cid"].asString();
  uid_ = json_pull_stream["uid"].asString();
  RTC_LOG(LS_INFO) << "RtdSignaling Response cid:" << cid_ << " uid:" << uid_;
  int code = root["code"].asInt64();
  if (code == 200) {
    RTC_LOG(LS_INFO) << "RtdSignaling consultation success.";
    Json::Value json_jsep = root["jsep"];
    std::string type = json_jsep["type"].asString();
    std::string answer_sdp_str = json_jsep["sdp"].asString();
    *answer_sdp = answer_sdp_str;
  } else {
    std::string err_msg = root["err_msg"].asString();
    RTC_LOG(LS_ERROR) << "RtdSignaling consultation failed. code:" << code << " err_msg:" << err_msg;
  }

  http_.reset(nullptr);
  return code;
}

int RtdSignaling::ConnectAndWaitResponseByWhip(std::string& offer_sdp, std::string* answer_sdp) {
  RTC_LOG(LS_INFO) << "RtdSignaling::ConnectAndWaitResponseByWhip";
  std::string url = whip_server_domain_ + "?streamUri=" + url_ + "&appkey=" + kTestAppkey;
  RTC_LOG(LS_INFO) << "RtdSignaling::ConnectAndWaitResponseByWhip url:" << url;
  RtdHttp http(url, timeout_ms_, kDefaultSignalingConnTimeoutMs);
  http.AddHeader("Content-Type", "application/sdp");
  http.AddContent(true, "", offer_sdp);

  int curl_code = http.DoEasy();
  if (curl_code != 0) {
    RTC_LOG(LS_ERROR) << "RtdSignaling::DoEasyPerform failed. code:" << curl_code;
    return -1;
  }

  std::string content = http.GetContent();
  RTC_LOG(LS_INFO) << "RtdSignaling::DoEasyPerform success. content:" << content;
  long http_status_code = http.GetHttpStatusCode();
  RTC_LOG(LS_INFO) << "RtdSignaling::DoEasyPerform success. http_status_code:" << http_status_code;
  if (http_status_code == 201) {
    *answer_sdp = content;
  } else {
    return http_status_code;
  }
  return 200;
}

} // namespace rtd
} // namespace webrtc
