#include "CServer.h"
#include "Logger.h"

CServer::CServer()
{
	m_server = NULL;
	m_business = NULL;
}

int CServer::Init(CBusiness* business, const Buffer& ip, short port)//��ʼ��������
{
	int ret = 0;
	if (business == NULL)return -1;
	m_business = business;
	ret = m_process.SetEntryFunction(&CBusiness::BusinessProcess, m_business);//����ҵ������
	if (ret != 0)return -2;
	ret = m_process.CreateSubProcess();//��������������
	if (ret != 0)return -3;
	ret = m_pool.Start(2);//�̳߳�����
	if (ret != 0)return -4;
	ret = m_epoll.Create(2);//epoll����
	if (ret != 0)return -5;
	m_server = new CSocket();//����һ���׽���
	if (m_server == NULL)return -6;
	ret = m_server->Init(CSockParam(ip, port, SOCK_ISSERVER | SOCK_ISIP)); //��ʼ���׽���
	if (ret != 0)return -7;
	ret = m_epoll.Add(*m_server, EpollData((void*)m_server)); //���׽��ּ��뵽epoll��
	if (ret != 0)return -8;
	for (size_t i = 0; i < m_pool.Size(); i++) {
		ret = m_pool.AddTask(&CServer::ThreadFunc, this); //�������
		if (ret != 0)return -9;
	}
	return 0;
}

int CServer::Run()
{
	while (m_server != NULL) {
		usleep(10);
	} //��������һֱ���У�ֱ���������ر�
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

 
