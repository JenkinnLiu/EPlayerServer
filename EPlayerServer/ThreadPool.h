#pragma once
#include "Epoll.h"
#include "Thread.h"
#include "Function.h"
#include "Socket.h"

class CThreadPool//线程池类
{
public:
	CThreadPool() { //构造函数
		m_server = NULL;
		timespec tp = { 0,0 }; 
		clock_gettime(CLOCK_REALTIME, &tp);//获取当前时间
		char* buf = NULL;
		asprintf(&buf, "%d.%d.sock", tp.tv_sec % 100000, tp.tv_nsec % 1000000); //生成一个唯一的名字
		if (buf != NULL) {
			m_path = buf;//将buf赋值给m_path
			free(buf);
		}//有问题的话，在start接口里面判断m_path来解决问题。
		usleep(1);//睡眠1微秒
	}
	~CThreadPool() {
		Close();
	}
	CThreadPool(const CThreadPool&) = delete;
	CThreadPool& operator=(const CThreadPool&) = delete;
public:
	int Start(unsigned count) { //启动线程池
		int ret = 0;
		if (m_server != NULL)return -1;//已经初始化了
		if (m_path.size() == 0)return -2;//构造函数失败！！！
		m_server = new CLocalSocket(); //创建一个本地套接字，用于监听与客户端的连接
		if (m_server == NULL)return -3;
		ret = m_server->Init(CSockParam(m_path, SOCK_ISSERVER));//本地套接字初始化
		if (ret != 0)return -4;
		ret = m_epoll.Create(count); //创建一个epoll
		if (ret != 0)return -5;
		ret = m_epoll.Add(*m_server, EpollData((void*)m_server)); //将m_server加入到epoll中
		if (ret != 0)return -6;
		m_threads.resize(count); //设置线程池的大小
		for (unsigned i = 0; i < count; i++) {
			m_threads[i] = new CThread(&CThreadPool::TaskDispatch, this); //创建一个线程，线程的入口函数是TaskDispatch，即任务分发函数
			if (m_threads[i] == NULL)return -7;
			ret = m_threads[i]->Start(); //启动线程
			if (ret != 0)return -8;
		}
		return 0;
	}
	void Close() {
		m_epoll.Close();
		if (m_server) {
			CSocketBase* p = m_server;
			m_server = NULL;
			delete p;
		}
		for (auto thread : m_threads)
		{
			if (thread)delete thread;
		}
		m_threads.clear();
		unlink(m_path);
	}
	template<typename _FUNCTION_, typename... _ARGS_>
	int AddTask(_FUNCTION_ func, _ARGS_... args) { //添加任务，即客户端发送数据给服务器
		static thread_local CLocalSocket client;//每个线程都有一个client，这里用static的原因是，
		//同一个线程多次调用AddTask时，只有第一次会初始化client，也就是说，
		// 同一个线程用的一样的client，不同的线程用的是不同的client
		int ret = 0;
		if (client == -1) {
			ret = client.Init(CSockParam(m_path, 0));//初始化client客户端
			if (ret != 0)return -1;
			ret = client.Link();
			if (ret != 0)return -2;
		}
		CFunctionBase* base = new CFunction< _FUNCTION_, _ARGS_...>(func, args...);//创建一个函数对象
		if (base == NULL)return -3;
		Buffer data(sizeof(base));
		memcpy(data, &base, sizeof(base));
		ret = client.Send(data); //客户端将函数对象data发送给服务器
		if (ret != 0) {
			delete base;
			return -4;
		}
		return 0;
	}
private:
	int TaskDispatch() {//任务分发函数
		while (m_epoll != -1) {
			EPEvents events;
			int ret = 0;
			ssize_t esize = m_epoll.WaitEvents(events); //等待事件
			if (esize > 0) {
				for (ssize_t i = 0; i < esize; i++) {
					if (events[i].events & EPOLLIN) {
						CSocketBase* pClient = NULL;
						if (events[i].data.ptr == m_server) {//如果说客户端请求连接

							ret = m_server->Link(&pClient); //接受客户端的连接
							if (ret != 0)continue;
							ret = m_epoll.Add(*pClient, EpollData((void*)pClient)); //将客户端加入到epoll中
							if (ret != 0) {
								delete pClient;
								continue;
							}
						}
						else {//客户端的数据来了
							pClient = (CSocketBase*)events[i].data.ptr;
							if (pClient) {
								CFunctionBase* base = NULL;
								Buffer data(sizeof(base));
								ret = pClient->Recv(data); //接受客户端的数据
								if (ret <= 0) {
									m_epoll.Del(*pClient);
									delete pClient;
									continue;
								}
								memcpy(&base, (char*)data, sizeof(base));
								if (base != NULL) {
									(*base)();
									delete base;
								}
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
	std::vector<CThread*> m_threads;
	CSocketBase* m_server;
	Buffer m_path;
};