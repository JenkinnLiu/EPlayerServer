#pragma once
#include "Socket.h"
#include "http_parser.h"
#include <map>

class CHttpParser//http������
{
private:
	http_parser m_parser; //http������
	http_parser_settings m_settings; //http���������ã������ص�����
	std::map<Buffer, Buffer> m_HeaderValues; //ͷ����ֵ��
	Buffer m_status; //״̬
	Buffer m_url; //url
	Buffer m_body; //body
	bool m_complete; //�Ƿ�������
	Buffer m_lastField; //���һ���ֶ�
public:
	CHttpParser();
	~CHttpParser();
	CHttpParser(const CHttpParser& http);
	CHttpParser& operator=(const CHttpParser& http);
public:
	size_t Parser(const Buffer& data);
	//GET POST ... �ο�http_parser.h HTTP_METHOD_MAP��
	unsigned Method() const { return m_parser.method; } //����http���󷽷�
	const std::map<Buffer, Buffer>& Headers() { return m_HeaderValues; } //����ͷ����ֵ��
	const Buffer& Status() const { return m_status; }
	const Buffer& Url() const { return m_url; }
	const Buffer& Body() const { return m_body; }
	unsigned Errno() const { return m_parser.http_errno; }
protected:
	static int OnMessageBegin(http_parser* parser); //��ʼ����ͷ��
	static int OnUrl(http_parser* parser, const char* at, size_t length); //����url
	static int OnStatus(http_parser* parser, const char* at, size_t length); //����״̬
	static int OnHeaderField(http_parser* parser, const char* at, size_t length); //����ͷ���ֶ�
	static int OnHeaderValue(http_parser* parser, const char* at, size_t length); //����ͷ��ֵ
	static int OnHeadersComplete(http_parser* parser); //ͷ���������
	static int OnBody(http_parser* parser, const char* at, size_t length); //����body
	static int OnMessageComplete(http_parser* parser); //�������
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
	//Ĭ�Ϸ���80
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