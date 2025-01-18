#include "Logger.h"

LogInfo::LogInfo(
	const char* file, int line, const char* func,
	pid_t pid, pthread_t tid, int level,
	const char* fmt, ...
)//有格式化字符串的构造函数
{
	const char sLevel[][8] = {
		"INFO","DEBUG","WARNING","ERROR","FATAL"
	};//日志级别
	char* buf = NULL; 
	bAuto = false;
	int count = asprintf(&buf, "%s(%d):[%s][%s]<%d-%d>(%s) ",
		file, line, sLevel[level],
		(char*)CLoggerServer::GetTimeStr(), pid, tid, func); //格式化字符串
	if (count > 0) {//如果格式化成功，将格式化字符串写入m_buf，释放buf
		m_buf = buf;
		free(buf);
	}
	else return;

	va_list ap;//可变参数列表
	va_start(ap, fmt);  //初始化可变参数列表
	//将格式化字符串 fmt 和可变参数列表 ap 格式化到一个新分配的字符串 buf 中。
	// vasprintf 会根据格式化后的字符串长度自动分配足够的内存，
	// 并返回格式化后的字符串长度 count。
	count = vasprintf(&buf, fmt, ap);
	if (count > 0) {
		m_buf += buf;
		free(buf);
	}
	m_buf += "\n";
	va_end(ap);
}

LogInfo::LogInfo(const char* file, int line, const char* func, pid_t pid, pthread_t tid, int level)
{//自己主动发的 流式的日志
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

	Buffer out;//缓冲区
	size_t i = 0;
	char* Data = (char*)pData;//数据指针
	for (; i < nSize; i++)
	{
		char buf[16] = "";
		//格式化输出，将数据写入buf，&0xFF是为了防止溢出去除高位数据
		snprintf(buf, sizeof(buf), "%02X ", Data[i] & 0xFF);
		m_buf += buf;//将buf写入m_buf
		if (0 == ((i + 1) % 16)) {//每16个字节换行
			m_buf += "\t; ";
			char buf[17] = "";
			memcpy(buf, Data + i - 15, 16);
			for (int j = 0; j < 16; j++) {
				if ((buf[j]) < 32 && (buf[j] >= 0)) buf[j] = '.';
			}
			m_buf += buf;
			/*for (size_t j = i - 15; j <= i; j++) { //循环遍历当前 16 个字节，生成字符表示。
				//如果字符是可打印字符（ASCII 范围 32-126），则追加到 m_buf。
				if ((Data[j] & 0xFF) > 31 && ((Data[j] & 0xFF) < 0x7F)) {
					m_buf += Data[i];
				}
				else {
					m_buf += '.';//否则追加点号作为占位符
				}
			}*/
			m_buf += "\n";
		}
	}
	//处理尾巴
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
