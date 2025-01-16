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
	EpollData() { m_data.u64 = 0; }//���캯������Ϊepoll_data_t��һ�������壬���Կ���ֱ�Ӹ�ֵ0
	EpollData(void* ptr) { m_data.ptr = ptr; }//���캯������ָ�븳ֵ��������
	explicit EpollData(int fd) { m_data.fd = fd; }//���캯������������ֵ��������,explicit��ֹ��ʽת��
	explicit EpollData(uint32_t u32) { m_data.u32 = u32; }//uint32_t��һ���޷�����������
	explicit EpollData(uint64_t u64) { m_data.u64 = u64; }
	EpollData(const EpollData& data) { m_data.u64 = data.m_data.u64; }//�������캯��
	EpollDatas& operator=(const EpollData& data) { 
		if (this != &data) 
			m_data.u64 = data.m_data.u64;
		return *this; 
	}//��ֵ���캯��
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
	operator epoll_data_t() { return m_data; }//����ת������
	operator epoll_data_t() const { return m_data; }//��������ת������
	operator epoll_data_t* () { return &m_data; }//ָ������ת������
	operator const epoll_data_t* () const { return &m_data; }//����ָ������ת������
private:
	epoll_data_t m_data; //epoll_data_t��һ�������壬���Դ��һ��ָ�����һ������
};

using EPEvents = std::vector<epoll_event>;

class CEpoll
{
public:
	CEpoll() {
		m_epoll = -1; //��ʼ��epoll���Ϊ-1
	}
	~CEpoll() {
		Close(); //���������йر�epoll
	}
	CEpoll(const CEpoll&) = delete;//��ֹ�������캯��
	CEpoll& operator=(const CEpoll&) = delete; //��ֹ��ֵ���캯��

	operator int() { return m_epoll; } //����ת������,������ת��Ϊint����
	int Create(unsigned count) {//����epoll
		if (m_epoll == -1) return -1; //���epoll�Ѿ����ڣ�����-1
		m_epoll = epoll_create(count); //����epoll
		if (m_epoll == -1) //����ʧ��
			return -2;
		return 0;
	}
	ssize_t WaitEvents(EPEvents& events, int timeout = 10) {//�ȴ��¼�
		if (m_epoll == -1) return -1;
		EPEvents evs[EVENT_SIZE]; //����һ���¼�����
		int ret = epoll_wait(m_epoll, evs.data(), (int)evs.size(), timeout); //�ȴ��¼�
		if (ret == -1) {
			if ((errno == EINTR) || (errno == EAGAIN)) return 0; //������жϻ��߳�ʱ������0
			return -2;
		}
		if (ret > (int)evs.size()) {
			evs.resize(ret); //����¼��������������С�����·��������С
		}
		memcpy(events.data(), evs.data(), sizeof(epoll_event) * ret); //���¼�������events��
		return ret;
	}
	int Add(int fd, const EpollData& data = EpollData((void*)0), uint32_t events = EPOLLIN) {//����¼�
		if (m_epoll == -1) return -1;
		epoll_event ev = { events, data }; //����һ���¼�
		int ret = epoll_ctl(m_epoll, EPOLL_CTL_ADD, fd, &ev); //����¼�
		if (ret == -1) return -2;
		return 0;
	}
	int Modify(int fd, uint32_t events, const EpollData& data = EpollData((void*)0)) {//�޸��¼�
		if (m_epoll == -1) return -1;
		epoll_event ev = { events, data }; //����һ���¼�
		int ret = epoll_ctl(m_epoll, EPOLL_CTL_MOD, fd, &ev); //�޸��¼�
		if (ret == -1) return -2;
		return 0;
	}
	int Del(int fd) {//ɾ���¼�
		if (m_epoll == -1) return -1;
		int ret = epoll_ctl(m_epoll, EPOLL_CTL_DEL, fd, NULL); //ɾ���¼�
		if (ret == -1) return -2;
		return 0;

	}
	void Close() {//�ر�epoll
		if (m_epoll != -1) {
			int fd = m_epoll;//�����Ա�̣���m_epoll��ֵ��fd����ֹm_epoll�����߳�
			m_epoll = -1;
			close(fd);
		}
	}
private:
	int m_epoll; //epoll���
};

