#pragma once
#include "Epoll.h"
#include "Thread.h"
#include "Function.h"
#include "Socket.h"

class CThreadPool//�̳߳���
{
public:
	CThreadPool() { //���캯��
		m_server = NULL;
		timespec tp = { 0,0 }; 
		clock_gettime(CLOCK_REALTIME, &tp);//��ȡ��ǰʱ��
		char* buf = NULL;
		asprintf(&buf, "%d.%d.sock", tp.tv_sec % 100000, tp.tv_nsec % 1000000); //����һ��Ψһ������
		if (buf != NULL) {
			m_path = buf;//��buf��ֵ��m_path
			free(buf);
		}//������Ļ�����start�ӿ������ж�m_path��������⡣
		usleep(1);//˯��1΢��
	}
	~CThreadPool() {
		Close();
	}
	CThreadPool(const CThreadPool&) = delete;
	CThreadPool& operator=(const CThreadPool&) = delete;
public:
	int Start(unsigned count) { //�����̳߳�
		int ret = 0;
		if (m_server != NULL)return -1;//�Ѿ���ʼ����
		if (m_path.size() == 0)return -2;//���캯��ʧ�ܣ�����
		m_server = new CLocalSocket(); //����һ�������׽��֣����ڼ�����ͻ��˵�����
		if (m_server == NULL)return -3;
		ret = m_server->Init(CSockParam(m_path, SOCK_ISSERVER));//�����׽��ֳ�ʼ��
		if (ret != 0)return -4;
		ret = m_epoll.Create(count); //����һ��epoll
		if (ret != 0)return -5;
		ret = m_epoll.Add(*m_server, EpollData((void*)m_server)); //��m_server���뵽epoll��
		if (ret != 0)return -6;
		m_threads.resize(count); //�����̳߳صĴ�С
		for (unsigned i = 0; i < count; i++) {
			m_threads[i] = new CThread(&CThreadPool::TaskDispatch, this); //����һ���̣߳��̵߳���ں�����TaskDispatch��������ַ�����
			if (m_threads[i] == NULL)return -7;
			ret = m_threads[i]->Start(); //�����߳�
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
	int AddTask(_FUNCTION_ func, _ARGS_... args) { //������񣬼��ͻ��˷������ݸ�������
		static thread_local CLocalSocket client;//ÿ���̶߳���һ��client��������static��ԭ���ǣ�
		//ͬһ���̶߳�ε���AddTaskʱ��ֻ�е�һ�λ��ʼ��client��Ҳ����˵��
		// ͬһ���߳��õ�һ����client����ͬ���߳��õ��ǲ�ͬ��client
		int ret = 0;
		if (client == -1) {
			ret = client.Init(CSockParam(m_path, 0));//��ʼ��client�ͻ���
			if (ret != 0)return -1;
			ret = client.Link();
			if (ret != 0)return -2;
		}
		CFunctionBase* base = new CFunction< _FUNCTION_, _ARGS_...>(func, args...);//����һ����������
		if (base == NULL)return -3;
		Buffer data(sizeof(base));
		memcpy(data, &base, sizeof(base));
		ret = client.Send(data); //�ͻ��˽���������data���͸�������
		if (ret != 0) {
			delete base;
			return -4;
		}
		return 0;
	}
private:
	int TaskDispatch() {//����ַ�����
		while (m_epoll != -1) {
			EPEvents events;
			int ret = 0;
			ssize_t esize = m_epoll.WaitEvents(events); //�ȴ��¼�
			if (esize > 0) {
				for (ssize_t i = 0; i < esize; i++) {
					if (events[i].events & EPOLLIN) {
						CSocketBase* pClient = NULL;
						if (events[i].data.ptr == m_server) {//���˵�ͻ�����������

							ret = m_server->Link(&pClient); //���ܿͻ��˵�����
							if (ret != 0)continue;
							ret = m_epoll.Add(*pClient, EpollData((void*)pClient)); //���ͻ��˼��뵽epoll��
							if (ret != 0) {
								delete pClient;
								continue;
							}
						}
						else {//�ͻ��˵���������
							pClient = (CSocketBase*)events[i].data.ptr;
							if (pClient) {
								CFunctionBase* base = NULL;
								Buffer data(sizeof(base));
								ret = pClient->Recv(data); //���ܿͻ��˵�����
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