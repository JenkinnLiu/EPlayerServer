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
	CThread() { //构造函数
		m_function = NULL; //函数指针
		m_thread = 0; //线程号
		m_bpaused = false; //是否暂停
	}

	template<typename _FUNCTION_, typename... _ARGS_>
	CThread(_FUNCTION_ func, _ARGS_... args)
		:m_function(new CFunction<_FUNCTION_, _ARGS_...>(func, args...)) //构造函数，初始化函数指针，传入function的参数
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
		m_function = new CFunction<_FUNCTION_, _ARGS_...>(func, args...); //设置函数指针
		if (m_function == NULL)return -1; //函数指针为空，返回-1
		return 0;
	}
	int Start() {
		pthread_attr_t attr; //线程属性
		int ret = 0;
		ret = pthread_attr_init(&attr); //初始化线程属性
		if (ret != 0)return -1;
		ret = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE); //设置线程属性，线程可被等待，JOINABLE是可等待的
		if (ret != 0)return -2;
		ret = pthread_attr_setscope(&attr, PTHREAD_SCOPE_PROCESS); //设置线程属性，线程的竞争范围为进程
		if (ret != 0)return -3;
		ret = pthread_create(&m_thread, &attr, &CThread::ThreadEntry, this); //创建线程，传入线程号、线程属性、线程入口函数、线程参数
		if (ret != 0)return -4;
		m_mapThread[m_thread] = this; //将线程号和线程指针放入map中
		ret = pthread_attr_destroy(&attr); //销毁线程属性
		if (ret != 0)return -5;
		return 0;
	}
	int Pause() { //暂停线程
		if (m_thread != 0)return -1;
		if (m_bpaused) {
			m_bpaused = false; //两次暂停等于继续
			return 0;
		}
		m_bpaused = true;
		int ret = pthread_kill(m_thread, SIGUSR1); //发送信号，暂停线程，SIGUSR1是用户自定义信号
		//进程收到信号后会执行信号处理函数
		if (ret != 0) {
			m_bpaused = false;
			return -2;
		}
		return 0;
	}
	int Stop() {
		if (m_thread != 0) { //如果线程号不为0，即无效
			pthread_t thread = m_thread; //将线程号赋值给thread
			m_thread = 0; //将线程号清零
			timespec ts;
			ts.tv_sec = 0;
			ts.tv_nsec = 100 * 1000000;//100ms
			int ret = pthread_timedjoin_np(thread, NULL, &ts); //等待线程结束，超时时间100ms
			if (ret == ETIMEDOUT) { //超时
				pthread_detach(thread); //分离线程
				pthread_kill(thread, SIGUSR2); //发送信号，线程退出，SIGUSR2是用户自定义信号
			}
		}
		return 0;
	}
	bool isValid()const { return m_thread == 0; } //线程号为0是才是有效线程
private:
	//__stdcall
	static void* ThreadEntry(void* arg) { // 线程入口函数
		CThread* thiz = (CThread*)arg; // 将传入的参数转换为 CThread 对象指针
		struct sigaction act = { 0 }; // 定义并初始化 sigaction 结构体
		sigemptyset(&act.sa_mask); // 初始化信号集为空
		act.sa_flags = SA_SIGINFO; // 设置标志位，使用扩展的信号处理函数
		act.sa_sigaction = &CThread::Sigaction; // 设置信号处理函数
		sigaction(SIGUSR1, &act, NULL); // 注册 SIGUSR1 信号的处理函数
		sigaction(SIGUSR2, &act, NULL); // 注册 SIGUSR2 信号的处理函数

		thiz->EnterThread(); // 调用对象的 EnterThread 方法，执行线程的主要功能

		if (thiz->m_thread) thiz->m_thread = 0; // 如果线程号不为 0，将其置为 0
		pthread_t thread = pthread_self(); // 获取当前线程的线程 ID
		auto it = m_mapThread.find(thread); // 在线程映射表中查找当前线程
		if (it != m_mapThread.end())
			m_mapThread[thread] = NULL; // 将当前线程从映射表中移除
		pthread_detach(thread); // 分离线程，使其资源在终止时自动释放
		pthread_exit(NULL); // 终止线程
	}

	static void Sigaction(int signo, siginfo_t* info, void* context)
	//信号处理函数设置成静态函数是因为信号处理函数是全局函数，不能访问类的成员变量和成员函数
	{
		if (signo == SIGUSR1) {//暂停信号
			pthread_t thread = pthread_self();
			auto it = m_mapThread.find(thread); //查找线程
			if (it != m_mapThread.end()) {
				if (it->second) {
					while (it->second->m_bpaused) {//循环休眠
						if (it->second->m_thread == 0) {
							pthread_exit(NULL);
						}
						usleep(1000);//1ms
					}
				}
			}
		}
		else if (signo == SIGUSR2) {//线程退出
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
	bool m_bpaused;//true 表示暂停 false表示运行中
	static std::map<pthread_t, CThread*> m_mapThread;
};