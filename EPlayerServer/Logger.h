#pragma once
#include "Thread.h"
#include "Epoll.h"
#include "Socket.h"
#include <list>
#include <sys/timeb.h>
#include <stdarg.h>
#include <sstream>
#include <sys/stat.h>

enum LogLevel {//��־���ͷּ�
	LOG_INFO,//��Ϣ
	LOG_DEBUG,//����		
	LOG_WARNING,//����
	LOG_ERROR,//����
	LOG_FATAL//��������
};

class LogInfo { //��־��Ϣ��
public:
	LogInfo(
		const char* file, int line, const char* func,
		pid_t pid, pthread_t tid, int level,
		const char* fmt, ...); //�ɱ�������캯����fmt�Ǹ�ʽ���ַ���
	LogInfo(
		const char* file, int line, const char* func,
		pid_t pid, pthread_t tid, int level);//��ͨ���캯��						
	LogInfo(const char* file, int line, const char* func,
		pid_t pid, pthread_t tid, int level,
		void* pData, size_t nSize); //�ڴ湹�캯����pData������ָ�룬nSize�����ݴ�С

	~LogInfo();
	operator Buffer()const {
		return m_buf;
	} //����ת����������LogInfoת��ΪBuffer�������־��Ϣ
	template<typename T>
	LogInfo& operator<<(const T& data) { //���������<<��ʵ����ʽ��־��������д��m_buf������LogInfo����ʵ����������<<����
		std::stringstream stream;
		stream << data;
		m_buf += stream.str(); //������д��m_buf
		return *this;
	}
private:
	bool bAuto;//Ĭ����false ��ʽ��־����Ϊtrue
	Buffer m_buf;//��־��Ϣ����
};

class CLoggerServer //��־��������
{
public:
	CLoggerServer() :
		m_thread(&CLoggerServer::ThreadFunc, this) //���ù��캯��ʱ��ֱ�ӳ�ʼ���߳�
	{
		m_server = NULL;
		char curpath[256] = "";
		getcwd(curpath, sizeof curpath);
		m_path = curpath;
		m_path = "./log/" + GetTimeStr() + ".log";//��־�ļ�·��
		printf("%s(%d):[%s]path=%s\n", __FILE__, __LINE__, __FUNCTION__, (char*)m_path);//��ӡ��־�ļ�·��
	}
	~CLoggerServer() {
		Close();
	}
public:
	CLoggerServer(const CLoggerServer&) = delete;//��ֹ�������캯��
	CLoggerServer& operator=(const CLoggerServer&) = delete; //��ֹ��ֵ�����
public:
	//��־������������
	int Start() {
		if (m_server != NULL)return -1;
		if (access("log", W_OK | R_OK) != 0) {
			mkdir("log", S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
		}//����log�ļ��У�S_IRUSR���û���Ȩ�ޣ�S_IWUSR���û�дȨ�ޣ�S_IRGRP�����Ȩ�ޣ�S_IWGRP����дȨ�ޣ�S_IROTH�������û���Ȩ��
		m_file = fopen(m_path, "w+"); //���ļ�
		if (m_file == NULL)return -2; 
		int ret = m_epoll.Create(1); //����epoll
		if (ret != 0)return -3;
		m_server = new CLocalSocket(); //���������׽���
		if (m_server == NULL) {
			Close(); 
			return -4;
		}
		ret = m_server->Init(CSockParam("./log/server.sock", (int)SOCK_ISSERVER));//��ʼ�������׽���
		if (ret != 0) {
			Close();
			return -5;
		}
		m_epoll.Add(*m_server, EpollData((void*)m_server), EPOLLIN | EPOLLERR); 
		ret = m_thread.Start();//�����߳�
		if (ret != 0) {
			Close();
			return -6;
		}
		return 0;
	}
	
	int Close() {
		if (m_server != NULL) {
			CSocketBase* p = m_server; //�رշ�����
			m_server = NULL;
			delete p; //ɾ��������
		}
		m_epoll.Close(); //�ر�epoll
		m_thread.Stop(); //�ر��߳�
		return 0;
	}
	//����������־���̵Ľ��̺��߳�ʹ�õ�
	static void Trace(const LogInfo& info) {//������־
		static thread_local CLocalSocket client;
		if (client == -1) {
			int ret = 0;
			ret = client.Init(CSockParam("./log/server.sock", 0)); //��ʼ����־������
			if (ret != 0) {
#ifdef _DEBUG
				printf("%s(%d):[%s]ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
#endif
				return;
			}
			ret = client.Link();
		}
		client.Send(info);//����־������������־
	}
	static Buffer GetTimeStr() {//��ȡʱ���ַ���
		Buffer result(128);
		timeb tmb;
		ftime(&tmb);
		tm* pTm = localtime(&tmb.time);
		int nSize = snprintf(result, result.size(),
			"%04d-%02d-%02d__%02d-%02d-%02d %03d",
			pTm->tm_year + 1900, pTm->tm_mon + 1, pTm->tm_mday,
			pTm->tm_hour, pTm->tm_min, pTm->tm_sec,
			tmb.millitm
		);
		result.resize(nSize);
		return result;
	}
private:
	int ThreadFunc() {//�̺߳���
		EPEvents events;
		std::map<int, CSocketBase*> mapClients; //�洢�ͻ����׽��ֵ�map
		while (m_thread.isValid() && (m_epoll != -1) && (m_server != NULL)) { //ѭ���ȴ��¼���ֱ���߳̽�������epoll�رջ��߷������ر�
			ssize_t ret = m_epoll.WaitEvents(events, 1); //�ȴ��¼�
			if (ret < 0)break;
			if (ret > 0) {
				ssize_t i = 0;
				for (; i < ret; i++) {
					if (events[i].events & EPOLLERR) { //���event�Ǵ������˳�
						break;
					}
					else if (events[i].events & EPOLLIN) { //���event�������¼�
						//����Ƿ������������¼��������ӿͻ��ˣ���Ϊ�������Ķ��¼����ҽ��пͻ�����������
						if (events[i].data.ptr == m_server) {
							CSocketBase* pClient = NULL;
							int r = m_server->Link(&pClient); //���ӿͻ���
							if (r < 0) continue;
							r = m_epoll.Add(*pClient, EpollData((void*)pClient), EPOLLIN | EPOLLERR); //���ͻ��˼���epoll
							if (r < 0) {
								delete pClient;
								continue;
							}
							auto it = mapClients.find(*pClient); //���ҿͻ���
							if (it->second != NULL) {
								delete it->second; //����ͻ����Ѿ����ڣ���ɾ��
							}
							mapClients[*pClient] = pClient; //�������ӿͻ��˼���map
						}
						else { //����ǿͻ��˵������¼�
							CSocketBase* pClient = (CSocketBase*)events[i].data.ptr; //��ȡ�ͻ���
							if (pClient != NULL) {
								Buffer data(1024 * 1024); //�������ݻ�����
								int r = pClient->Recv(data); //��������
								if (r <= 0) {
									delete pClient;
									mapClients[*pClient] = NULL;
								}
								else {
									WriteLog(data); //д����־
								}
							}
						}
					}
				}
				if (i != ret) {
					break;
				}
			}
		}
		for (auto it = mapClients.begin(); it != mapClients.end(); it++) {
			if (it->second) {
				delete it->second; //�˳�ѭ��ʱ�ͷ����пͻ���
			}
		}
		mapClients.clear(); //���map
		return 0;
	}
	void WriteLog(const Buffer& data) { //д����־
		if (m_file != NULL) {
			FILE* pFile = m_file; //��ʼ���ļ�ָ��
			fwrite((char*)data, 1, data.size(), pFile); //���ļ�д������
			fflush(pFile); //ˢ���ļ�
#ifdef _DEBUG
			printf("%s", (char*)data);
#endif
		}
	}
private:
	CThread m_thread; //�߳�
	CEpoll m_epoll; //epoll
	CSocketBase* m_server; //�������׽���
	Buffer m_path; //��־·��
	FILE* m_file; //�ļ�ָ��
};

#ifndef TRACE //TRACE�����͸������͵���־
#define TRACEI(...) CLoggerServer::Trace(LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_INFO, __VA_ARGS__))
#define TRACED(...) CLoggerServer::Trace(LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_DEBUG, __VA_ARGS__))
#define TRACEW(...) CLoggerServer::Trace(LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_WARNING, __VA_ARGS__))
#define TRACEE(...) CLoggerServer::Trace(LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_ERROR, __VA_ARGS__))
#define TRACEF(...) CLoggerServer::Trace(LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_FATAL, __VA_ARGS__))

//LOGI<<"hello"<<"how are you"; //��ʽ��־
#define LOGI LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_INFO)
#define LOGD LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_DEBUG)
#define LOGW LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_WARNING)
#define LOGE LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_ERROR)
#define LOGF LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_FATAL)

//�ڴ浼����DUMP��־
//00 01 02 65����  ; ...a����
//
#define DUMPI(data, size) CLoggerServer::Trace(LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_INFO, data, size))
#define DUMPD(data, size) CLoggerServer::Trace(LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_DEBUG, data, size))
#define DUMPW(data, size) CLoggerServer::Trace(LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_WARNING, data, size))
#define DUMPE(data, size) CLoggerServer::Trace(LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_ERROR, data, size))
#define DUMPF(data, size) CLoggerServer::Trace(LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_FATAL, data, size))
#endif
