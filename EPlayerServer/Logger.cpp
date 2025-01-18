#include "Logger.h"

LogInfo::LogInfo(
	const char* file, int line, const char* func,
	pid_t pid, pthread_t tid, int level,
	const char* fmt, ...
)//�и�ʽ���ַ����Ĺ��캯��
{
	const char sLevel[][8] = {
		"INFO","DEBUG","WARNING","ERROR","FATAL"
	};//��־����
	char* buf = NULL; 
	bAuto = false;
	int count = asprintf(&buf, "%s(%d):[%s][%s]<%d-%d>(%s) ",
		file, line, sLevel[level],
		(char*)CLoggerServer::GetTimeStr(), pid, tid, func); //��ʽ���ַ���
	if (count > 0) {//�����ʽ���ɹ�������ʽ���ַ���д��m_buf���ͷ�buf
		m_buf = buf;
		free(buf);
	}
	else return;

	va_list ap;//�ɱ�����б�
	va_start(ap, fmt);  //��ʼ���ɱ�����б�
	//����ʽ���ַ��� fmt �Ϳɱ�����б� ap ��ʽ����һ���·�����ַ��� buf �С�
	// vasprintf ����ݸ�ʽ������ַ��������Զ������㹻���ڴ棬
	// �����ظ�ʽ������ַ������� count��
	count = vasprintf(&buf, fmt, ap);
	if (count > 0) {
		m_buf += buf;
		free(buf);
	}
	m_buf += "\n";
	va_end(ap);
}

LogInfo::LogInfo(const char* file, int line, const char* func, pid_t pid, pthread_t tid, int level)
{//�Լ��������� ��ʽ����־
	bAuto = true;
	const char sLevel[][8] = {
		"INFO","DEBUG","WARNING","ERROR","FATAL"
	};
	char* buf = NULL;
	int count = asprintf(&buf, "%s(%d):[%s][%s]<%d-%d>(%s) ",
		file, line, sLevel[level],
		(char*)CLoggerServer::GetTimeStr(), pid, tid, func);
	if (count > 0) {
		m_buf = buf;
		free(buf);
	}
}

LogInfo::LogInfo(
	const char* file, int line, const char* func,
	pid_t pid, pthread_t tid, int level,
	void* pData, size_t nSize
) 
{
	const char sLevel[][8] = {
		"INFO","DEBUG","WARNING","ERROR","FATAL"
	};
	char* buf = NULL;
	bAuto = false;
	int count = asprintf(&buf, "%s(%d):[%s][%s]<%d-%d>(%s)\n",
		file, line, sLevel[level],
		(char*)CLoggerServer::GetTimeStr(), pid, tid, func);
	if (count > 0) {
		m_buf = buf;
		free(buf);
	}
	else return;

	Buffer out;//������
	size_t i = 0;
	char* Data = (char*)pData;//����ָ��
	for (; i < nSize; i++)
	{
		char buf[16] = "";
		//��ʽ�������������д��buf��&0xFF��Ϊ�˷�ֹ���ȥ����λ����
		snprintf(buf, sizeof(buf), "%02X ", Data[i] & 0xFF);
		m_buf += buf;//��bufд��m_buf
		if (0 == ((i + 1) % 16)) {//ÿ16���ֽڻ���
			m_buf += "\t; ";
			char buf[17] = "";
			memcpy(buf, Data + i - 15, 16);
			for (int j = 0; j < 16; j++) {
				if ((buf[j]) < 32 && (buf[j] >= 0)) buf[j] = '.';
			}
			m_buf += buf;
			/*for (size_t j = i - 15; j <= i; j++) { //ѭ��������ǰ 16 ���ֽڣ������ַ���ʾ��
				//����ַ��ǿɴ�ӡ�ַ���ASCII ��Χ 32-126������׷�ӵ� m_buf��
				if ((Data[j] & 0xFF) > 31 && ((Data[j] & 0xFF) < 0x7F)) {
					m_buf += Data[i];
				}
				else {
					m_buf += '.';//����׷�ӵ����Ϊռλ��
				}
			}*/
			m_buf += "\n";
		}
	}
	//����β��
	size_t k = i % 16;
	if (k != 0) {
		for (size_t j = 0; j < 16 - k; j++) m_buf += "   ";
		m_buf += "\t; ";
		for (size_t j = i - k; j <= i; j++) {
			if ((Data[i] & 0xFF) > 31 && ((Data[j] & 0xFF) < 0x7F)) {
				m_buf += Data[i];
			}
			else {
				m_buf += '.';
			}
		}
		m_buf += "\n";
	}
}

LogInfo::~LogInfo()
{
	if (bAuto) {
		m_buf += "\n";
		CLoggerServer::Trace(*this);
	}
}
