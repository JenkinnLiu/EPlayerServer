#include "HttpParser.h"

CHttpParser::CHttpParser() //���캯��
{
	m_complete = false;
	memset(&m_parser, 0, sizeof(m_parser)); //��ʼ��http������
	m_parser.data = this;
	http_parser_init(&m_parser, HTTP_REQUEST);
	memset(&m_settings, 0, sizeof(m_settings));
	m_settings.on_message_begin = &CHttpParser::OnMessageBegin; //���ûص����������涼�ǻص�����
	m_settings.on_url = &CHttpParser::OnUrl; //����url�ص�����
	m_settings.on_status = &CHttpParser::OnStatus; //����״̬�ص�����
	m_settings.on_header_field = &CHttpParser::OnHeaderField; //����ͷ���ֶλص�����
	m_settings.on_header_value = &CHttpParser::OnHeaderValue; //����ͷ��ֵ�ص�����
	m_settings.on_headers_complete = &CHttpParser::OnHeadersComplete; //����ͷ��������ɻص�����
	m_settings.on_body = &CHttpParser::OnBody; //����body�ص�����
	m_settings.on_message_complete = &CHttpParser::OnMessageComplete; //���ý�����ɻص�����
}

CHttpParser::~CHttpParser()
{}

CHttpParser::CHttpParser(const CHttpParser& http) //�������캯��
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

CHttpParser& CHttpParser::operator=(const CHttpParser& http) //��ֵ���������
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

size_t CHttpParser::Parser(const Buffer& data) //����http����
{
	m_complete = false;
	size_t ret = http_parser_execute(
		&m_parser, &m_settings, data, data.size());//ִ��http����,��http_parser_execute������http���������ص����������ݡ����ݳ���
	if (m_complete == false) {
		m_parser.http_errno = 0x7F;
		return 0;
	}
	return ret;
}

int CHttpParser::OnMessageBegin(http_parser* parser)//��ʼ����ͷ��
{
	return ((CHttpParser*)parser->data)->OnMessageBegin();//parser->dataָ�����CHttpParser��ʵ��
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
	m_url = Buffer(at, length);//����url������url����m_url��Buffer��һ��string��
	return 0;
}

int CHttpParser::OnStatus(const char* at, size_t length)
{
	m_status = Buffer(at, length);//����״̬������״̬����m_status
	return 0;
}

int CHttpParser::OnHeaderField(const char* at, size_t length)
{
	m_lastField = Buffer(at, length);
	return 0;
}

int CHttpParser::OnHeaderValue(const char* at, size_t length)
{
	m_HeaderValues[m_lastField] = Buffer(at, length);//����ͷ����ֵ�ԣ�������ֵ�Դ���m_HeaderValues
	return 0;
}

int CHttpParser::OnHeadersComplete()
{
	return 0;
}

int CHttpParser::OnBody(const char* at, size_t length)
{
	m_body = Buffer(at, length);//����body������body����m_body
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

int UrlParser::Parser()//�����ַ�������
{
	//��������Э�顢�����Ͷ˿ڡ�uri����ֵ��
	//����Э��
	const char* pos = m_url;//posָ��url���׵�ַ
	const char* target = strstr(pos, "://"); //targetָ��://
	if (target == NULL)return -1;
	m_protocol = Buffer(pos, target); //�洢��pos��ʼ��target���ַ���
	//���������Ͷ˿�
	pos = target + 3;//posָ������
	target = strchr(pos, '/'); //targetָ��/
	if (target == NULL) {
		if (m_protocol.size() + 3 >= m_url.size()) 
			return -2;
		m_host = pos;//�洢����
		return 0;
	}
	Buffer value = Buffer(pos, target);
	if (value.size() == 0)return -3;
	target = strchr(value, ':'); //targetָ��:
	if (target != NULL) {
		m_host = Buffer(value, target);//�洢����
		m_port = atoi(Buffer(target + 1, (char*)value + value.size()));//�洢�˿�
	}
	else {
		m_host = value;
	}
	pos = strchr(pos, '/'); //posָ��/
	//����uri
	target = strchr(pos, '?');//targetָ��?����Ϊ�ʺź����Ǽ�ֵ��
	if (target == NULL) {
		m_uri = pos;
		return 0;
	}
	else {
		m_uri = Buffer(pos, target);
		//����key��value������ֵ��
		pos = target + 1;
		const char* t = NULL;
		do {
			target = strchr(pos, '&');//��ֵ�Եķָ�����&
			if (target == NULL) {//���һ����ֵ��
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

Buffer UrlParser::operator[](const Buffer& name) const//���ؼ�ֵ��
{
	auto it = m_values.find(name);
	if (it == m_values.end())return Buffer();
	return it->second;
}

void UrlParser::SetUrl(const Buffer& url)//����url
{
	m_url = url;
	m_protocol = "";
	m_host = "";
	m_port = 80;
	m_values.clear();
}
