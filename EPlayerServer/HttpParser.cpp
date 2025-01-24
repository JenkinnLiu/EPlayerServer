#include "HttpParser.h"

CHttpParser::CHttpParser() //构造函数
{
	m_complete = false;
	memset(&m_parser, 0, sizeof(m_parser)); //初始化http解析器
	m_parser.data = this;
	http_parser_init(&m_parser, HTTP_REQUEST);
	memset(&m_settings, 0, sizeof(m_settings));
	m_settings.on_message_begin = &CHttpParser::OnMessageBegin; //设置回调函数，下面都是回调函数
	m_settings.on_url = &CHttpParser::OnUrl; //设置url回调函数
	m_settings.on_status = &CHttpParser::OnStatus; //设置状态回调函数
	m_settings.on_header_field = &CHttpParser::OnHeaderField; //设置头部字段回调函数
	m_settings.on_header_value = &CHttpParser::OnHeaderValue; //设置头部值回调函数
	m_settings.on_headers_complete = &CHttpParser::OnHeadersComplete; //设置头部解析完成回调函数
	m_settings.on_body = &CHttpParser::OnBody; //设置body回调函数
	m_settings.on_message_complete = &CHttpParser::OnMessageComplete; //设置解析完成回调函数
}

CHttpParser::~CHttpParser()
{}

CHttpParser::CHttpParser(const CHttpParser& http) //拷贝构造函数
{
	memcpy(&m_parser, &http.m_parser, sizeof(m_parser));
	m_parser.data = this;
	memcpy(&m_settings, &http.m_settings, sizeof(m_settings));
	m_status = http.m_status;
	m_url = http.m_url;
	m_body = http.m_body;
	m_complete = http.m_complete;
	m_lastField = http.m_lastField;
}

CHttpParser& CHttpParser::operator=(const CHttpParser& http) //赋值运算符重载
{
	if (this != &http) {
		memcpy(&m_parser, &http.m_parser, sizeof(m_parser));
		m_parser.data = this;
		memcpy(&m_settings, &http.m_settings, sizeof(m_settings));
		m_status = http.m_status;
		m_url = http.m_url;
		m_body = http.m_body;
		m_complete = http.m_complete;
		m_lastField = http.m_lastField;
	}
	return *this;
}

size_t CHttpParser::Parser(const Buffer& data) //解析http请求
{
	m_complete = false;
	size_t ret = http_parser_execute(
		&m_parser, &m_settings, data, data.size());//执行http解析,即http_parser_execute，传入http解析器、回调函数、数据、数据长度
	if (m_complete == false) {
		m_parser.http_errno = 0x7F;
		return 0;
	}
	return ret;
}

int CHttpParser::OnMessageBegin(http_parser* parser)//开始解析头部
{
	return ((CHttpParser*)parser->data)->OnMessageBegin();//parser->data指向的是CHttpParser的实例
}

int CHttpParser::OnUrl(http_parser* parser, const char* at, size_t length)
{
	return ((CHttpParser*)parser->data)->OnUrl(at, length);
}

int CHttpParser::OnStatus(http_parser* parser, const char* at, size_t length)
{
	return ((CHttpParser*)parser->data)->OnStatus(at, length);
}

int CHttpParser::OnHeaderField(http_parser* parser, const char* at, size_t length)
{
	return ((CHttpParser*)parser->data)->OnHeaderField(at, length);
}

int CHttpParser::OnHeaderValue(http_parser* parser, const char* at, size_t length)
{
	return ((CHttpParser*)parser->data)->OnHeaderValue(at, length);
}

int CHttpParser::OnHeadersComplete(http_parser* parser)
{
	return ((CHttpParser*)parser->data)->OnHeadersComplete();
}

int CHttpParser::OnBody(http_parser* parser, const char* at, size_t length)
{
	return ((CHttpParser*)parser->data)->OnBody(at, length);
}

int CHttpParser::OnMessageComplete(http_parser* parser)
{
	return ((CHttpParser*)parser->data)->OnMessageComplete();
}

int CHttpParser::OnMessageBegin()
{
	return 0;
}

int CHttpParser::OnUrl(const char* at, size_t length)
{
	m_url = Buffer(at, length);//解析url，即将url存入m_url，Buffer是一个string，
	return 0;
}

int CHttpParser::OnStatus(const char* at, size_t length)
{
	m_status = Buffer(at, length);//解析状态，即将状态存入m_status
	return 0;
}

int CHttpParser::OnHeaderField(const char* at, size_t length)
{
	m_lastField = Buffer(at, length);
	return 0;
}

int CHttpParser::OnHeaderValue(const char* at, size_t length)
{
	m_HeaderValues[m_lastField] = Buffer(at, length);//解析头部键值对，即将键值对存入m_HeaderValues
	return 0;
}

int CHttpParser::OnHeadersComplete()
{
	return 0;
}

int CHttpParser::OnBody(const char* at, size_t length)
{
	m_body = Buffer(at, length);//解析body，即将body存入m_body
	return 0;
}

int CHttpParser::OnMessageComplete()
{
	m_complete = true;
	return 0;
}

UrlParser::UrlParser(const Buffer& url)
{
	m_url = url;
}

int UrlParser::Parser()//就是字符串处理
{
	//分三步：协议、域名和端口、uri、键值对
	//解析协议
	const char* pos = m_url;//pos指向url的首地址
	const char* target = strstr(pos, "://"); //target指向://
	if (target == NULL)return -1;
	m_protocol = Buffer(pos, target); //存储从pos开始到target的字符串
	//解析域名和端口
	pos = target + 3;//pos指向域名
	target = strchr(pos, '/'); //target指向/
	if (target == NULL) {
		if (m_protocol.size() + 3 >= m_url.size()) 
			return -2;
		m_host = pos;//存储域名
		return 0;
	}
	Buffer value = Buffer(pos, target);
	if (value.size() == 0)return -3;
	target = strchr(value, ':'); //target指向:
	if (target != NULL) {
		m_host = Buffer(value, target);//存储域名
		m_port = atoi(Buffer(target + 1, (char*)value + value.size()));//存储端口
	}
	else {
		m_host = value;
	}
	pos = strchr(pos, '/'); //pos指向/
	//解析uri
	target = strchr(pos, '?');//target指向?，因为问号后面是键值对
	if (target == NULL) {
		m_uri = pos;
		return 0;
	}
	else {
		m_uri = Buffer(pos, target);
		//解析key和value，即键值对
		pos = target + 1;
		const char* t = NULL;
		do {
			target = strchr(pos, '&');//键值对的分隔符是&
			if (target == NULL) {//最后一个键值对
				t = strchr(pos, '=');
				if (t == NULL)return -4;
				m_values[Buffer(pos, t)] = Buffer(t + 1);
			}
			else {
				Buffer kv(pos, target);
				t = strchr(kv, '=');
				if (t == NULL)return -5;
				m_values[Buffer(kv, t)] = Buffer(t + 1, kv + kv.size());
				pos = target + 1;
			}
		} while (target != NULL);
	}

	return 0;
}

Buffer UrlParser::operator[](const Buffer& name) const//返回键值对
{
	auto it = m_values.find(name);
	if (it == m_values.end())return Buffer();
	return it->second;
}

void UrlParser::SetUrl(const Buffer& url)//设置url
{
	m_url = url;
	m_protocol = "";
	m_host = "";
	m_port = 80;
	m_values.clear();
}
