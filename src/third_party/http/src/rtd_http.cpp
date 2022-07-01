#include "rtd_http.h"

#ifndef CURL_STATICLIB
#define CURL_STATICLIB
#endif
#include "curl/curl.h"

#if defined(WIN32)
#if defined(DEBUG) || defined(_DEBUG)
#pragma comment(lib, "libcurld.lib")
#else
#pragma comment(lib, "libcurl.lib")
#endif
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "wldap32.lib")
#endif

#define MAX_CONTENT_SIZE 20000

int RtdHttp::count_ = 0;

RtdHttp::RtdHttp(const std::string & url, int timeout, int conn_timeout_ms) 
    : url_(url) {
  if (count_++ == 0) {
    curl_global_init(CURL_GLOBAL_ALL);
  }

  curl_handle_ = curl_easy_init();
  if (!curl_handle_) {
    return;
  }

  if (timeout > 0) {
    curl_easy_setopt(curl_handle_, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl_handle_, CURLOPT_TIMEOUT_MS, timeout);
    curl_easy_setopt(curl_handle_, CURLOPT_CONNECTTIMEOUT_MS, conn_timeout_ms);
    curl_easy_setopt(curl_handle_, CURLOPT_VERBOSE, 1L);
  }

  curl_easy_setopt(curl_handle_, CURLOPT_URL, url.c_str());
  SetSharedHandler();

  initialized_ = true;
}

RtdHttp::~RtdHttp() {
  if (curl_list_) {
    curl_slist_free_all(curl_list_);
  }

  if (curl_handle_) {
    curl_easy_cleanup(curl_handle_);
  }

  if (--count_ <= 0) {
    curl_global_cleanup();
    count_ = 0;
  }
}

void RtdHttp::AddHeader(const std::string& name, const std::string& value) {
  std::string header = name + ":" + value;
  curl_list_ = curl_slist_append(curl_list_, header.c_str());
  curl_easy_setopt(curl_handle_, CURLOPT_HTTPHEADER, curl_list_);
}

void RtdHttp::AddContent(bool post, const std::string& form_post, const std::string& post_field) {
  if (!form_post.empty()) {
    curl_easy_setopt(curl_handle_, CURLOPT_HTTPPOST, form_post.c_str());
  } else {
    if (!post_field.empty()) {
      curl_easy_setopt(curl_handle_, CURLOPT_POSTFIELDS, post_field.c_str());
    }
  }

  if (url_.substr(0, 8) == "https://") {
    curl_easy_setopt(curl_handle_, CURLOPT_SSL_VERIFYHOST, 2);
    curl_easy_setopt(curl_handle_, CURLOPT_SSL_VERIFYPEER, 0);
  }

  curl_easy_setopt(curl_handle_, CURLOPT_WRITEDATA, this);
  curl_easy_setopt(curl_handle_, CURLOPT_WRITEFUNCTION, RtdHttp::WriteMemory);
}

void RtdHttp::SetSharedHandler() {
  static CURLSH* shared_handler = nullptr;
  if (!shared_handler) {
    shared_handler = curl_share_init();
    curl_share_setopt(shared_handler, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);
  }
  curl_easy_setopt(curl_handle_, CURLOPT_SHARE, shared_handler);
  curl_easy_setopt(curl_handle_, CURLOPT_DNS_CACHE_TIMEOUT, 60*5);
}

int RtdHttp::DoEasy() {
  CURLcode code = CURL_LAST;
  if (curl_handle_) {
    code = curl_easy_perform(curl_handle_);
  }
  return code;
}

std::string RtdHttp::GetContent() {
  return content_;
}

long RtdHttp::GetHttpStatusCode() {
  long http_code = -1;
  if (curl_handle_) {
    curl_easy_getinfo(curl_handle_, CURLINFO_RESPONSE_CODE, &http_code);
  }
  return http_code;
}

size_t RtdHttp::WriteMemory(void* data, size_t size, size_t count, void * param) {
  if (data == nullptr) {
    return 0;
  }

  size_t data_bytes = size * count;
  RtdHttp* rtd_http = (RtdHttp *)param;
  int total_bytes = rtd_http->content_bytes_ + data_bytes;
  if (total_bytes > MAX_CONTENT_SIZE) {
    return data_bytes;
  }
  rtd_http->content_.append((const char*)data, data_bytes);
  rtd_http->content_bytes_ = total_bytes;
  return data_bytes;
}
