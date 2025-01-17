#pragma once
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <signal.h>
#include <map>
#include "Function.h"


class CThread
{
public:
	CThread() { //���캯��
		m_function = NULL; //����ָ��
		m_thread = 0; //�̺߳�
		m_bpaused = false; //�Ƿ���ͣ
	}

	template<typename _FUNCTION_, typename... _ARGS_>
	CThread(_FUNCTION_ func, _ARGS_... args)
		:m_function(new CFunction<_FUNCTION_, _ARGS_...>(func, args...)) //���캯������ʼ������ָ�룬����function�Ĳ���
	{
		m_thread = 0;
		m_bpaused = false;
	}

	~CThread() {}
public:
	CThread(const CThread&) = delete;
	CThread operator=(const CThread&) = delete;
public:
	template<typename _FUNCTION_, typename... _ARGS_>
	int SetThreadFunc(_FUNCTION_ func, _ARGS_... args)
	{
		m_function = new CFunction<_FUNCTION_, _ARGS_...>(func, args...); //���ú���ָ��
		if (m_function == NULL)return -1; //����ָ��Ϊ�գ�����-1
		return 0;
	}
	int Start() {
		pthread_attr_t attr; //�߳�����
		int ret = 0;
		ret = pthread_attr_init(&attr); //��ʼ���߳�����
		if (ret != 0)return -1;
		ret = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE); //�����߳����ԣ��߳̿ɱ��ȴ���JOINABLE�ǿɵȴ���
		if (ret != 0)return -2;
		ret = pthread_attr_setscope(&attr, PTHREAD_SCOPE_PROCESS); //�����߳����ԣ��̵߳ľ�����ΧΪ����
		if (ret != 0)return -3;
		ret = pthread_create(&m_thread, &attr, &CThread::ThreadEntry, this); //�����̣߳������̺߳š��߳����ԡ��߳���ں������̲߳���
		if (ret != 0)return -4;
		m_mapThread[m_thread] = this; //���̺߳ź��߳�ָ�����map��
		ret = pthread_attr_destroy(&attr); //�����߳�����
		if (ret != 0)return -5;
		return 0;
	}
	int Pause() { //��ͣ�߳�
		if (m_thread != 0)return -1;
		if (m_bpaused) {
			m_bpaused = false; //������ͣ���ڼ���
			return 0;
		}
		m_bpaused = true;
		int ret = pthread_kill(m_thread, SIGUSR1); //�����źţ���ͣ�̣߳�SIGUSR1���û��Զ����ź�
		//�����յ��źź��ִ���źŴ�����
		if (ret != 0) {
			m_bpaused = false;
			return -2;
		}
		return 0;
	}
	int Stop() {
		if (m_thread != 0) { //����̺߳Ų�Ϊ0������Ч
			pthread_t thread = m_thread; //���̺߳Ÿ�ֵ��thread
			m_thread = 0; //���̺߳�����
			timespec ts;
			ts.tv_sec = 0;
			ts.tv_nsec = 100 * 1000000;//100ms
			int ret = pthread_timedjoin_np(thread, NULL, &ts); //�ȴ��߳̽�������ʱʱ��100ms
			if (ret == ETIMEDOUT) { //��ʱ
				pthread_detach(thread); //�����߳�
				pthread_kill(thread, SIGUSR2); //�����źţ��߳��˳���SIGUSR2���û��Զ����ź�
			}
		}
		return 0;
	}
	bool isValid()const { return m_thread == 0; } //�̺߳�Ϊ0�ǲ�����Ч�߳�
private:
	//__stdcall
	static void* ThreadEntry(void* arg) { // �߳���ں���
		CThread* thiz = (CThread*)arg; // ������Ĳ���ת��Ϊ CThread ����ָ��
		struct sigaction act = { 0 }; // ���岢��ʼ�� sigaction �ṹ��
		sigemptyset(&act.sa_mask); // ��ʼ���źż�Ϊ��
		act.sa_flags = SA_SIGINFO; // ���ñ�־λ��ʹ����չ���źŴ�����
		act.sa_sigaction = &CThread::Sigaction; // �����źŴ�����
		sigaction(SIGUSR1, &act, NULL); // ע�� SIGUSR1 �źŵĴ�����
		sigaction(SIGUSR2, &act, NULL); // ע�� SIGUSR2 �źŵĴ�����

		thiz->EnterThread(); // ���ö���� EnterThread ������ִ���̵߳���Ҫ����

		if (thiz->m_thread) thiz->m_thread = 0; // ����̺߳Ų�Ϊ 0��������Ϊ 0
		pthread_t thread = pthread_self(); // ��ȡ��ǰ�̵߳��߳� ID
		auto it = m_mapThread.find(thread); // ���߳�ӳ����в��ҵ�ǰ�߳�
		if (it != m_mapThread.end())
			m_mapThread[thread] = NULL; // ����ǰ�̴߳�ӳ������Ƴ�
		pthread_detach(thread); // �����̣߳�ʹ����Դ����ֹʱ�Զ��ͷ�
		pthread_exit(NULL); // ��ֹ�߳�
	}

	static void Sigaction(int signo, siginfo_t* info, void* context)
	//�źŴ��������óɾ�̬��������Ϊ�źŴ�������ȫ�ֺ��������ܷ�����ĳ�Ա�����ͳ�Ա����
	{
		if (signo == SIGUSR1) {//��ͣ�ź�
			pthread_t thread = pthread_self();
			auto it = m_mapThread.find(thread); //�����߳�
			if (it != m_mapThread.end()) {
				if (it->second) {
					while (it->second->m_bpaused) {//ѭ������
						if (it->second->m_thread == 0) {
							pthread_exit(NULL);
						}
						usleep(1000);//1ms
					}
				}
			}
		}
		else if (signo == SIGUSR2) {//�߳��˳�
			pthread_exit(NULL);
		}
	}

	void EnterThread() {//__thiscall
		if (m_function != NULL) {
			int ret = (*m_function)();
			if (ret != 0) {
				printf("%s(%d):[%s]ret = %d\n", __FILE__, __LINE__, __FUNCTION__);
			}
		}
	}
private:
	CFunctionBase* m_function;
	pthread_t m_thread;
	bool m_bpaused;//true ��ʾ��ͣ false��ʾ������
	static std::map<pthread_t, CThread*> m_mapThread;
};