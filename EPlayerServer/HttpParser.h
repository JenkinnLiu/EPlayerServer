#pragma once
#include "Socket.h"
#include "http_parser.h"
#include <map>

class CHttpParser//http解析类
{
private:
	http_parser m_parser; //http解析器
	http_parser_settings m_settings; //http解析器设置，包括回调函数
	std::map<Buffer, Buffer> m_HeaderValues; //头部键值对
	Buffer m_status; //状态
	Buffer m_url; //url
	Buffer m_body; //body
	bool m_complete; //是否解析完成
	Buffer m_lastField; //最后一个字段
public:
	CHttpParser();
	~CHttpParser();
	CHttpParser(const CHttpParser& http);
	CHttpParser& operator=(const CHttpParser& http);
public:
	size_t Parser(const Buffer& data);
	//GET POST ... 参考http_parser.h HTTP_METHOD_MAP宏
	unsigned Method() const { return m_parser.method; } //返回http请求方法
	const std::map<Buffer, Buffer>& Headers() { return m_HeaderValues; } //返回头部键值对
	const Buffer& Status() const { return m_status; }
	const Buffer& Url() const { return m_url; }
	const Buffer& Body() const { return m_body; }
	unsigned Errno() const { return m_parser.http_errno; }
protected:
	static int OnMessageBegin(http_parser* parser); //开始解析头部
	static int OnUrl(http_parser* parser, const char* at, size_t length); //解析url
	static int OnStatus(http_parser* parser, const char* at, size_t length); //解析状态
	static int OnHeaderField(http_parser* parser, const char* at, size_t length); //解析头部字段
	static int OnHeaderValue(http_parser* parser, const char* at, size_t length); //解析头部值
	static int OnHeadersComplete(http_parser* parser); //头部解析完成
	static int OnBody(http_parser* parser, const char* at, size_t length); //解析body
	static int OnMessageComplete(http_parser* parser); //解析完成
	int OnMessageBegin();
	int OnUrl(const char* at, size_t length);
	int OnStatus(const char* at, size_t length);
	int OnHeaderField(const char* at, size_t length);
	int OnHeaderValue(const char* at, size_t length);
	int OnHeadersComplete();
	int OnBody(const char* at, size_t length);
	int OnMessageComplete();
};

class UrlParser
{
public:
	UrlParser(const Buffer& url);
	~UrlParser() {}
	int Parser();
	Buffer operator[](const Buffer& name)const;
	Buffer Protocol()const { return m_protocol; }
	Buffer Host()const { return m_host; }
	//默认返回80
	int Port()const { return m_port; }
	void SetUrl(const Buffer& url);
private:
	Buffer m_url;
	Buffer m_protocol;
	Buffer m_host;
	Buffer m_uri;
	int m_port;
	std::map<Buffer, Buffer> m_values;
};