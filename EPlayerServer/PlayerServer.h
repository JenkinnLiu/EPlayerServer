#pragma once
#include "Logger.h"
#include "CServer.h"
#include <map>
/*
* 1. 客户端的地址问题
* 2. 连接回调的参数问题
* 3. 接收回调的参数问题
*/
#define ERR_RETURN(ret, err) if(ret!=0){TRACEE("ret= %d errno = %d msg = [%s]", ret, errno, strerror(errno));return err;}

#define WARN_CONTINUE(ret) if(ret!=0){TRACEW("ret= %d errno = %d msg = [%s]", ret, errno, strerror(errno));continue;}

class CEdoyunPlayerServer ://主模块服务器类
	public CBusiness
{
public:
	CEdoyunPlayerServer(unsigned count) :CBusiness() {//构造函数，传入线程数
		m_count = count;
	}
	~CEdoyunPlayerServer() {
		m_epoll.Close();
		m_pool.Close();
		for (auto it : m_mapClients) {//删除map中的客户端
			if (it.second) {
				delete it.second;
			}
		}
		m_mapClients.clear();
	}
	virtual int BusinessProcess(CProcess* proc) {//业务处理进程函数
		using namespace std::placeholders;//占位符，placeholders::_1表示第一个参数,这样可以把形参抽象化，更方便解耦，编译器只要知道是第几个参数就行
		ret = setConnectedCallback(&CEdoyunPlayerServer::Connected, this, _1);//设置连接回调函数
		ERR_RETURN(ret, -1);
		ret = setRecvCallback(&CEdoyunPlayerServer::Received, this, _1, _2);//设置接收回调函数
		ERR_RETURN(ret, -2);
		ret = m_epoll.Create(m_count);//创建epoll
		ERR_RETURN(ret, -1);
		ret = m_pool.Start(m_count);//启动线程池
		ERR_RETURN(ret, -2);
		for (unsigned i = 0; i < m_count; i++) {
			ret = m_pool.AddTask(&CEdoyunPlayerServer::ThreadFunc, this);//添加任务
			ERR_RETURN(ret, -3);
		}
		int sock = 0;
		sockaddr_in addrin;
		while (m_epoll != -1) {
			ret = proc->RecvSocket(sock, &addrin);//子进程接收socket和客户端文件描述符
			if (ret < 0 || (sock == 0))break;
			CSocketBase* pClient = new CSocket(sock);//根据文件描述符创建客户端并对其进行操作
			if (pClient == NULL)continue;
			ret = pClient->Init(CSockParam(&addrin, SOCK_ISIP));//初始化客户端
			WARN_CONTINUE(ret);
			ret = m_epoll.Add(sock, EpollData((void*)pClient));//将客户端添加到epoll中
			if (m_connectedcallback) {
				(*m_connectedcallback)(pClient);//将客户端作为形参传入连接回调函数
			}
			WARN_CONTINUE(ret);
		}
		return 0;
	}
private:
	int Connected(CSocketBase* pClient) {
		return 0;
	}
	int Received(CSocketBase* pClient, const Buffer& data) {
		return 0;
	}
private:
	int ThreadFunc() {//线程函数
		int ret = 0;
		EPEvents events;
		while (m_epoll != -1) {
			ssize_t size = m_epoll.WaitEvents(events);//等待事件
			if (size < 0)break;
			if (size > 0) {
				for (ssize_t i = 0; i < size; i++)//遍历epoll传进来的事件
				{
					if (events[i].events & EPOLLERR) {
						break;
					}
					else if (events[i].events & EPOLLIN) {//如果有客户端数据信号传进来
						CSocketBase* pClient = (CSocketBase*)events[i].data.ptr;//获取客户端
						if (pClient) {
							Buffer data;
							ret = pClient->Recv(data);//接收数据
							WARN_CONTINUE(ret);
							if (m_recvcallback) {
								(*m_recvcallback)(pClient, data);
							}
						}
					}
				}
			}
		}
		return 0;
	}
private:
	CEpoll m_epoll;
	std::map<int, CSocketBase*> m_mapClients;
	CThreadPool m_pool;
	unsigned m_count;
};