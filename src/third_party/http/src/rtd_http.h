#ifndef RTD_HTTP_H_
#define RTD_HTTP_H_

#include <string>

struct curl_slist;

class RtdHttp
{
public:
	RtdHttp(const std::string& url, int timeout);
	~RtdHttp();
	bool IsInitialized() { return initialized_; }

	void AddHeader(const std::string& name, const std::string& value);
	void AddContent(bool post, const std::string& form_post, const std::string& post_field);
	void SetSharedHandler();
	int  DoEasy();
	std::string GetContent();

private:
	static size_t WriteMemory(void *data, size_t size, size_t count, void* param);

private:
	static int count_;
	bool initialized_ = false;
	std::string url_;
	std::string content_;
	int content_bytes_ = 0;
	void* curl_handle_ = nullptr;
	curl_slist* curl_list_ = nullptr;
};

#endif