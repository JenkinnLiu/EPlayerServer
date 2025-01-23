#include "CServer.h"
#include "Logger.h"

CServer::CServer()
{
	m_server = NULL;
	m_business = NULL;
}

int CServer::Init(CBusiness* business, const Buffer& ip, short port)//初始化服务器
{
	int ret = 0;
	if (business == NULL)return -1;
	m_business = business;
	ret = m_process.SetEntryFunction(&CBusiness::BusinessProcess, m_business);//设置业务处理函数
	if (ret != 0)return -2;
	ret = m_process.CreateSubProcess();//创建服务器进程
	if (ret != 0)return -3;
	ret = m_pool.Start(2);//线程池启动
	if (ret != 0)return -4;
	ret = m_epoll.Create(2);//epoll创建
	if (ret != 0)return -5;
	m_server = new CSocket();//创建一个套接字
	if (m_server == NULL)return -6;
	ret = m_server->Init(CSockParam(ip, port, SOCK_ISSERVER | SOCK_ISIP)); //初始化套接字
	if (ret != 0)return -7;
	ret = m_epoll.Add(*m_server, EpollData((void*)m_server)); //将套接字加入到epoll中
	if (ret != 0)return -8;
	for (size_t i = 0; i < m_pool.Size(); i++) {
		ret = m_pool.AddTask(&CServer::ThreadFunc, this); //添加任务
		if (ret != 0)return -9;
	}
	return 0;
}

int CServer::Run()
{
	while (m_server != NULL) {
		usleep(10);
	} //服务器会一直运行，直到服务器关闭
	return 0;
}

int CServer::Close()
{
	if (m_server) {
		CSocketBase* sock = m_server;
		m_server = NULL;
		m_epoll.Del(*sock);
		delete sock;
	}
	m_epoll.Close();
	m_process.SendFD(-1);
	m_pool.Close();
	return 0;
}

int CServer::ThreadFunc()
{
	int ret = 0;
	EPEvents events;
	while ((m_epoll != -1) && (m_server != NULL)) {//服务器运行
		ssize_t size = m_epoll.WaitEvents(events);//等待事件
		if (size < 0)break;
		if (size > 0) {
			for (ssize_t i = 0; i < size; i++)
			{
				if (events[i].events & EPOLLERR) {
					break;
				}
				else if (events[i].events & EPOLLIN) {//服务器接收到客户端的连接
					if (m_server) {
						CSocketBase* pClient = NULL;
						ret = m_server->Link(&pClient);//连接客户端
						if (ret != 0)continue;
						ret = m_process.SendSocket(*pClient, *pClient);//向子进程（业务模块）发送客户端的文件描述符，子进程（业务模块）接收到客户端的文件描述符后，可以与客户端通信
						delete pClient;
						if (ret != 0) {
							TRACEE("send client %d failed!", (int)*pClient);
							continue;
						}
					}
				}
			}
		}
	}
	return 0;
}
