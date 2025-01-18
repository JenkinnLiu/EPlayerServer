#pragma once
#include "Thread.h"
#include "Epoll.h"
#include "Socket.h"
#include <list>
#include <sys/timeb.h>
#include <stdarg.h>
#include <sstream>
#include <sys/stat.h>

enum LogLevel {//日志类型分级
	LOG_INFO,//信息
	LOG_DEBUG,//调试		
	LOG_WARNING,//警告
	LOG_ERROR,//错误
	LOG_FATAL//致命错误
};

class LogInfo { //日志信息类
public:
	LogInfo(
		const char* file, int line, const char* func,
		pid_t pid, pthread_t tid, int level,
		const char* fmt, ...); //可变参数构造函数，fmt是格式化字符串
	LogInfo(
		const char* file, int line, const char* func,
		pid_t pid, pthread_t tid, int level);//普通构造函数						
	LogInfo(const char* file, int line, const char* func,
		pid_t pid, pthread_t tid, int level,
		void* pData, size_t nSize); //内存构造函数，pData是数据指针，nSize是数据大小

	~LogInfo();
	operator Buffer()const {
		return m_buf;
	} //类型转换函数，将LogInfo转换为Buffer，输出日志信息
	template<typename T>
	LogInfo& operator<<(const T& data) { //重载运算符<<，实现流式日志，将数据写入m_buf，返回LogInfo可以实现连续调用<<符号
		std::stringstream stream;
		stream << data;
		m_buf += stream.str(); //将数据写入m_buf
		return *this;
	}
private:
	bool bAuto;//默认是false 流式日志，则为true
	Buffer m_buf;//日志信息数据
};

class CLoggerServer //日志服务器类
{
public:
	CLoggerServer() :
		m_thread(&CLoggerServer::ThreadFunc, this) //调用构造函数时，直接初始化线程
	{
		m_server = NULL;
		char curpath[256] = "";
		getcwd(curpath, sizeof curpath);
		m_path = curpath;
		m_path = "./log/" + GetTimeStr() + ".log";//日志文件路径
		printf("%s(%d):[%s]path=%s\n", __FILE__, __LINE__, __FUNCTION__, (char*)m_path);//打印日志文件路径
	}
	~CLoggerServer() {
		Close();
	}
public:
	CLoggerServer(const CLoggerServer&) = delete;//禁止拷贝构造函数
	CLoggerServer& operator=(const CLoggerServer&) = delete; //禁止赋值运算符
public:
	//日志服务器的启动
	int Start() {
		if (m_server != NULL)return -1;
		if (access("log", W_OK | R_OK) != 0) {
			mkdir("log", S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
		}//创建log文件夹，S_IRUSR是用户读权限，S_IWUSR是用户写权限，S_IRGRP是组读权限，S_IWGRP是组写权限，S_IROTH是其他用户读权限
		m_file = fopen(m_path, "w+"); //打开文件
		if (m_file == NULL)return -2; 
		int ret = m_epoll.Create(1); //创建epoll
		if (ret != 0)return -3;
		m_server = new CLocalSocket(); //创建本地套接字
		if (m_server == NULL) {
			Close(); 
			return -4;
		}
		ret = m_server->Init(CSockParam("./log/server.sock", (int)SOCK_ISSERVER));//初始化本地套接字
		if (ret != 0) {
			Close();
			return -5;
		}
		m_epoll.Add(*m_server, EpollData((void*)m_server), EPOLLIN | EPOLLERR); 
		ret = m_thread.Start();//启动线程
		if (ret != 0) {
			Close();
			return -6;
		}
		return 0;
	}
	
	int Close() {
		if (m_server != NULL) {
			CSocketBase* p = m_server; //关闭服务器
			m_server = NULL;
			delete p; //删除服务器
		}
		m_epoll.Close(); //关闭epoll
		m_thread.Stop(); //关闭线程
		return 0;
	}
	//给其他非日志进程的进程和线程使用的
	static void Trace(const LogInfo& info) {//发送日志
		static thread_local CLocalSocket client;
		if (client == -1) {
			int ret = 0;
			ret = client.Init(CSockParam("./log/server.sock", 0)); //初始化日志服务器
			if (ret != 0) {
#ifdef _DEBUG
				printf("%s(%d):[%s]ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
#endif
				return;
			}
			ret = client.Link();
		}
		client.Send(info);//向日志服务器发送日志
	}
	static Buffer GetTimeStr() {//获取时间字符串
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
	int ThreadFunc() {//线程函数
		EPEvents events;
		std::map<int, CSocketBase*> mapClients; //存储客户端套接字的map
		while (m_thread.isValid() && (m_epoll != -1) && (m_server != NULL)) { //循环等待事件，直到线程结束或者epoll关闭或者服务器关闭
			ssize_t ret = m_epoll.WaitEvents(events, 1); //等待事件
			if (ret < 0)break;
			if (ret > 0) {
				ssize_t i = 0;
				for (; i < ret; i++) {
					if (events[i].events & EPOLLERR) { //如果event是错误，则退出
						break;
					}
					else if (events[i].events & EPOLLIN) { //如果event是输入事件
						//如果是服务器的输入事件，则连接客户端，因为服务器的读事件有且仅有客户端请求连接
						if (events[i].data.ptr == m_server) {
							CSocketBase* pClient = NULL;
							int r = m_server->Link(&pClient); //连接客户端
							if (r < 0) continue;
							r = m_epoll.Add(*pClient, EpollData((void*)pClient), EPOLLIN | EPOLLERR); //将客户端加入epoll
							if (r < 0) {
								delete pClient;
								continue;
							}
							auto it = mapClients.find(*pClient); //查找客户端
							if (it->second != NULL) {
								delete it->second; //如果客户端已经存在，则删除
							}
							mapClients[*pClient] = pClient; //将新连接客户端加入map
						}
						else { //如果是客户端的输入事件
							CSocketBase* pClient = (CSocketBase*)events[i].data.ptr; //获取客户端
							if (pClient != NULL) {
								Buffer data(1024 * 1024); //创建数据缓冲区
								int r = pClient->Recv(data); //接收数据
								if (r <= 0) {
									delete pClient;
									mapClients[*pClient] = NULL;
								}
								else {
									WriteLog(data); //写入日志
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
				delete it->second; //退出循环时释放所有客户端
			}
		}
		mapClients.clear(); //清空map
		return 0;
	}
	void WriteLog(const Buffer& data) { //写入日志
		if (m_file != NULL) {
			FILE* pFile = m_file; //初始化文件指针
			fwrite((char*)data, 1, data.size(), pFile); //向文件写入数据
			fflush(pFile); //刷新文件
#ifdef _DEBUG
			printf("%s", (char*)data);
#endif
		}
	}
private:
	CThread m_thread; //线程
	CEpoll m_epoll; //epoll
	CSocketBase* m_server; //服务器套接字
	Buffer m_path; //日志路径
	FILE* m_file; //文件指针
};

#ifndef TRACE //TRACE：发送各种类型的日志
#define TRACEI(...) CLoggerServer::Trace(LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_INFO, __VA_ARGS__))
#define TRACED(...) CLoggerServer::Trace(LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_DEBUG, __VA_ARGS__))
#define TRACEW(...) CLoggerServer::Trace(LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_WARNING, __VA_ARGS__))
#define TRACEE(...) CLoggerServer::Trace(LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_ERROR, __VA_ARGS__))
#define TRACEF(...) CLoggerServer::Trace(LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_FATAL, __VA_ARGS__))

//LOGI<<"hello"<<"how are you"; //流式日志
#define LOGI LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_INFO)
#define LOGD LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_DEBUG)
#define LOGW LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_WARNING)
#define LOGE LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_ERROR)
#define LOGF LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_FATAL)

//内存导出的DUMP日志
//00 01 02 65……  ; ...a……
//
#define DUMPI(data, size) CLoggerServer::Trace(LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_INFO, data, size))
#define DUMPD(data, size) CLoggerServer::Trace(LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_DEBUG, data, size))
#define DUMPW(data, size) CLoggerServer::Trace(LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_WARNING, data, size))
#define DUMPE(data, size) CLoggerServer::Trace(LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_ERROR, data, size))
#define DUMPF(data, size) CLoggerServer::Trace(LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_FATAL, data, size))
#endif
