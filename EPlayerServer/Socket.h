#pragma once
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string>
#include <fcntl.h>
#include "Public.h"


enum SockAttr {
	SOCK_ISSERVER = 1,//是否服务器 1表示是 0表示客户端
	SOCK_ISNONBLOCK = 2,//是否阻塞 1表示非阻塞 0表示阻塞
	SOCK_ISUDP = 4,//是否为UDP 1表示udp 0表示tcp
	SOCK_ISIP = 8;//是否为IP协议 1表示IP协议 0表示本地UNIX域套接字
};

class CSockParam { //套接字参数类
public:
	CSockParam() {
		bzero(&addr_in, sizeof(addr_in)); //初始化，将addr_in的前sizeof(addr_in)个字节置为0
		bzero(&addr_un, sizeof(addr_un));
		port = -1;
		attr = 0;//默认是客户端、阻塞、tcp
	}
	CSockParam(const Buffer& ip, short port, int attr) {//参数为ip、端口、属性
		this->ip = ip; //设置ip
		this->port = port; 
		this->attr = attr; //设置属性（客户端、阻塞、tcp）
		addr_in.sin_family = AF_INET; //设置地址族为IPv4
		addr_in.sin_port = port; //设置端口
		addr_in.sin_addr.s_addr = inet_addr(ip); //将ip转换为网络字节序
	}
	CSockParam(const Buffer& path, int attr) {//UNIX域套接字，参数为路径和属性
		ip = path;
		addr_un.sun_family = AF_UNIX; //本地套接字写法：AF_UNIX，使用addr_un
		strcpy(addr_un.sun_path, path);
		this->attr = attr;
	}
	~CSockParam() {}
	CSockParam(const CSockParam& param) {//拷贝构造函数
		ip = param.ip;
		port = param.port;
		attr = param.attr;
		memcpy(&addr_in, &param.addr_in, sizeof(addr_in));
	}
	CSockParam(const sockaddr_in* addrin, int attr) {//拷贝构造函数
		ip = path;
		addr_un.sun_family = AF_UNIX; //本地套接字写法：AF_UNIX，使用addr_un
		strcpy(addr_un.sun_path, path);
		this->attr = attr;
	}
public:
	CSockParam& operator=(const CSockParam& param) {//赋值运算符重载
		if (this != &param) {
			ip = param.ip;
			port = param.port;
			attr = param.attr;
			memcpy(&addr_in, &param.addr_in, sizeof(addr_in));
			memcpy(&addr_un, &param.addr_un, sizeof(addr_un));
		}
		return *this;
	}
	sockaddr* addrin() { return (sockaddr*)&addr_in; } //返回addr_in
	sockaddr* addrun() { return (sockaddr*)&addr_un; }
public:
	//地址
	sockaddr_in addr_in;
	sockaddr_un addr_un;
	//ip
	Buffer ip;
	//端口
	short port;
	//参考SockAttr
	int attr;
};

class CSocketBase
{
public:
	CSocketBase() {
		m_socket = -1;
		m_status = 0;//初始化未完成
	}
	//传递析构操作
	virtual ~CSocketBase() {
		Close();
	}
public:
	//初始化 服务器 套接字创建、bind、listen  客户端 套接字创建
	virtual int Init(const CSockParam& param) = 0;
	//连接 服务器 accept 客户端 connect  对于udp这里可以忽略
	virtual int Link(CSocketBase** pClient = NULL) = 0;
	//发送数据
	virtual int Send(const Buffer& data) = 0;
	//接收数据
	virtual int Recv(Buffer& data) = 0;
	//关闭连接
	virtual int Close() {
		m_status = 3;
		if (m_socket != -1) {
			if (m_param.attr & SOCK_ISSERVER && (m_param & SOCK_ISIP) == 0) {
				unlink(m_param.ip);
			}
			int fd = m_socket;
			m_socket = -1;
			close(fd);
		}
	};
protected:
	//套接字描述符，默认是-1
	int m_socket;
	//状态 0初始化未完成 1初始化完成 2连接完成 3已经关闭
	int m_status;
	//初始化参数
	CSockParam m_param;
};

class CSocket
	:public CSocketBase
{
public:
	CSocket() :CSocketBase() {}
	CSocket(int sock) :CSocketBase() {
		m_socket = sock;
	}
	//传递析构操作
	virtual ~CSocket() {
		Close();
	}
public:
	//初始化 服务器 套接字创建、bind、listen  客户端 套接字创建
	virtual int Init(const CSockParam& param) {
		if (m_status != 0)return -1;
		m_param = param;
		int type = (m_param.attr & SOCK_ISUDP) ? SOCK_DGRAM : SOCK_STREAM;
		if (m_socket == -1) {
			if (param.attr & SOCK_ISIP)
				m_socket = socket(PF_INET, type, 0);//创建一个IP协议的套接字
			else
				m_socket = socket(PF_LOCAL, type, 0);//创建一个本地套接字
			m_socket = socket(PF_LOCAL, type, 0);

		}		
		else {
			m_status = 2;//accept来的套接字，已经处于连接状态
		}
		if (m_socket == -1)return -2;
		int ret = 0;
		if (m_param.attr & SOCK_ISSERVER) {
			if (m_param.attr & SOCK_ISIP)
				ret = bind(m_socket, m_param.addrin(), sizeof(sockaddr_in));//绑定IP地址
			else {
				ret = bind(m_socket, m_param.addrun(), sizeof(sockaddr_un));//绑定本地套接字
			}
			if (ret == -1) return -3;
			ret = listen(m_socket, 32);
			if (ret == -1)return -4;
			
		}
		if (m_param.attr & SOCK_ISNONBLOCK) {
			int option = fcntl(m_socket, F_GETFL); //获取文件状态标志
			if (option == -1)return -5;
			option |= O_NONBLOCK;
			ret = fcntl(m_socket, F_SETFL, option);
			if (ret == -1)return -6;
		}
		if(m_status == 0)
			m_status = 1;
		return 0;
	}
	//连接 服务器 accept 客户端 connect  对于udp这里可以忽略
	virtual int Link(CSocketBase** pClient = NULL) { //连接，这里用双指针是为了返回一个新的客户端
		if (m_status <= 0 || (m_socket == -1))return -1;
		int ret = 0;
		if (m_param.attr & SOCK_ISSERVER) { //服务器
			if (pClient == NULL)return -2;
			CSockParam param;
			int fd = -1;
			socklen_t len = 0;
			if (m_param.attr & SOCK_ISIP) {//IP协议
				param.attr |= SOCK_ISIP;
				len = sizeof(sockaddr_in);
				fd = accept(m_socket, param.addrin(), &len);
				if (fd == -1)return -3;
			}
			else {
				len = sizeof(sockaddr_un);
				fd = accept(m_socket, param.addrun(), &len);
				if (fd == -1)return -3;
			}
			*pClient = new CSocket(fd); //创建一个新的客户端
			if (*pClient == NULL)return -4;
			ret = (*pClient)->Init(param); //初始化客户端
			if (ret != 0) {
				delete (*pClient);
				*pClient = NULL;
				return -5;
			}
		}
		else { //客户端
			ret = connect(m_socket, m_param.addrun(), sizeof(sockaddr_un));
			if (ret != 0)return -6;
		}
		m_status = 2;
		return 0;
	}
	//发送数据
	virtual int Send(const Buffer& data) {
		if (m_status < 2 || (m_socket == -1))return -1;
		ssize_t index = 0;
		while (index < (ssize_t)data.size()) {
			ssize_t len = write(m_socket, (char*)data + index, data.size() - index);
			if (len == 0)return -2;
			if (len < 0)return -3;
			index += len;
		}
		return 0;
	}
	//接收数据 大于零，表示接收成功 小于 表示失败 等于0 表示没有收到数据，但没有错误
	virtual int Recv(Buffer& data) {
		if (m_status < 2 || (m_socket == -1))return -1;
		ssize_t len = read(m_socket, data, data.size());
		if (len > 0) {
			data.resize(len);
			return (int)len;//收到数据
		}
		if (len < 0) {
			if (errno == EINTR || (errno == EAGAIN)) {
				data.clear();
				return 0;//没有数据收到
			}
			return -2;//发送错误
		}
		return -3;//套接字被关闭
	}
	//关闭连接
	virtual int Close() {
		return CSocketBase::Close();
	}
};