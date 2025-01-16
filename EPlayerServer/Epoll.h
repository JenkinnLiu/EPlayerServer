#pragma once

#include <unistd.h>
#include<sys/epoll.h>
#include <vector>
#include<errno.h>
#include<sys/signal.h>
#include<memory.h>
#define EVENT_SIZE 128

class EpollData {
public:
	EpollData() { m_data.u64 = 0; }//构造函数，因为epoll_data_t是一个联合体，所以可以直接赋值0
	EpollData(void* ptr) { m_data.ptr = ptr; }//构造函数，将指针赋值给联合体
	explicit EpollData(int fd) { m_data.fd = fd; }//构造函数，将整数赋值给联合体,explicit防止隐式转换
	explicit EpollData(uint32_t u32) { m_data.u32 = u32; }//uint32_t是一个无符号整数类型
	explicit EpollData(uint64_t u64) { m_data.u64 = u64; }
	EpollData(const EpollData& data) { m_data.u64 = data.m_data.u64; }//拷贝构造函数
	EpollDatas& operator=(const EpollData& data) { 
		if (this != &data) 
			m_data.u64 = data.m_data.u64;
		return *this; 
	}//赋值构造函数
	EpollDatas& operator=(void* data) {
		m_data.ptr = data;
		return *this;
	}
	EpollDatas& operator=(int data) {
		m_data.fd = data;
		return *this;
	}
	EpollDatas& operator=(uint32_t data) {
		m_data.u32 = data;
		return *this;
	}
	EpollDatas& operator=(uint64_t data) {
		m_data.u64 = data;
		return *this;
	}
	operator epoll_data_t() { return m_data; }//类型转换函数
	operator epoll_data_t() const { return m_data; }//常量类型转换函数
	operator epoll_data_t* () { return &m_data; }//指针类型转换函数
	operator const epoll_data_t* () const { return &m_data; }//常量指针类型转换函数
private:
	epoll_data_t m_data; //epoll_data_t是一个联合体，可以存放一个指针或者一个整数
};

using EPEvents = std::vector<epoll_event>;

class CEpoll
{
public:
	CEpoll() {
		m_epoll = -1; //初始化epoll句柄为-1
	}
	~CEpoll() {
		Close(); //析构函数中关闭epoll
	}
	CEpoll(const CEpoll&) = delete;//禁止拷贝构造函数
	CEpoll& operator=(const CEpoll&) = delete; //禁止赋值构造函数

	operator int() { return m_epoll; } //类型转换函数,将对象转换为int类型
	int Create(unsigned count) {//创建epoll
		if (m_epoll == -1) return -1; //如果epoll已经存在，返回-1
		m_epoll = epoll_create(count); //创建epoll
		if (m_epoll == -1) //创建失败
			return -2;
		return 0;
	}
	ssize_t WaitEvents(EPEvents& events, int timeout = 10) {//等待事件
		if (m_epoll == -1) return -1;
		EPEvents evs[EVENT_SIZE]; //创建一个事件数组
		int ret = epoll_wait(m_epoll, evs.data(), (int)evs.size(), timeout); //等待事件
		if (ret == -1) {
			if ((errno == EINTR) || (errno == EAGAIN)) return 0; //如果是中断或者超时，返回0
			return -2;
		}
		if (ret > (int)evs.size()) {
			evs.resize(ret); //如果事件数量大于数组大小，重新分配数组大小
		}
		memcpy(events.data(), evs.data(), sizeof(epoll_event) * ret); //将事件拷贝到events中
		return ret;
	}
	int Add(int fd, const EpollData& data = EpollData((void*)0), uint32_t events = EPOLLIN) {//添加事件
		if (m_epoll == -1) return -1;
		epoll_event ev = { events, data }; //创建一个事件
		int ret = epoll_ctl(m_epoll, EPOLL_CTL_ADD, fd, &ev); //添加事件
		if (ret == -1) return -2;
		return 0;
	}
	int Modify(int fd, uint32_t events, const EpollData& data = EpollData((void*)0)) {//修改事件
		if (m_epoll == -1) return -1;
		epoll_event ev = { events, data }; //创建一个事件
		int ret = epoll_ctl(m_epoll, EPOLL_CTL_MOD, fd, &ev); //修改事件
		if (ret == -1) return -2;
		return 0;
	}
	int Del(int fd) {//删除事件
		if (m_epoll == -1) return -1;
		int ret = epoll_ctl(m_epoll, EPOLL_CTL_DEL, fd, NULL); //删除事件
		if (ret == -1) return -2;
		return 0;

	}
	void Close() {//关闭epoll
		if (m_epoll != -1) {
			int fd = m_epoll;//防御性编程，将m_epoll赋值给fd，防止m_epoll被多线程
			m_epoll = -1;
			close(fd);
		}
	}
private:
	int m_epoll; //epoll句柄
};

