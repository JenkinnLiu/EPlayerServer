#pragma once
#include "function.h"
#include <cstdio>
#include <memory.h>
#include <sys/socket.h>‘
#include<sys/stat.h>
#include<fcntl.h>
#include<signal.h>

class CProcess
{
public:
	CProcess() {
		m_func = NULL;
		memset(pipes, 0, sizeof(pipes));//管道，作用是进程间通信
	}
	~CProcess() {
		if (m_func != NULL) {
			delete m_func;
			m_func = NULL;
		}
	}

	template<typename _FUNCTION_, typename... _ARGS_>
	int SetEntryFunction(_FUNCTION_ func, _ARGS_... args)
	{
		m_func = new CFunction<_FUNCTION_, _ARGS_...>(func, args...); //创建一个函数对象
		return 0;
	}

	int CreateSubProcess() {
		if (m_func == NULL)return -1;
		int ret = socketpair(AF_LOCAL, SOCK_STREAM, 0, pipes);
		if (ret == -1)return -2;
		pid_t pid = fork();
		if (pid == -1)return -3;
		if (pid == 0) {
			//子进程
			close(pipes[1]);//关闭掉写，子进程只读管道
			pipes[1] = 0;
			ret = (*m_func)();
			exit(0);
		}
		//主进程
		close(pipes[0]);//关闭掉读，主进程只写管道
		pipes[0] = 0;
		m_pid = pid;
		return 0;
	}

	//msghdr、iovec 和 cmsghdr 是在网络编程中用于消息传递的结构体，主要用于高级的套接字操作，
	// 特别是在使用 sendmsg 和 recvmsg 函数时。

	//msghdr 结构体用于描述一个消息，包括消息的地址、缓冲区和控制信息。它通常用于 sendmsg 和 recvmsg 函数。
	//struct msghdr {
	//	void* msg_name;       // 可选的地址
	//	socklen_t     msg_namelen;    // 地址长度
	//	struct iovec* msg_iov;        // 数据缓冲区数组
	//	int           msg_iovlen;     // 数据缓冲区数组的长度
	//	void* msg_control;    // 可选的附加数据（控制信息）
	//	socklen_t     msg_controllen; // 附加数据的长度
	//	int           msg_flags;      // 消息标志
	//};

	//iovec 结构体用于描述一个缓冲区。它通常与 msghdr 结构体一起使用，用于指定消息的多个缓冲区。
	//struct iovec {
	//	void* iov_base; // 缓冲区的起始地址
	//	size_t iov_len;  // 缓冲区的长度
	//};
	//cmsghdr 结构体用于描述控制信息（如文件描述符传递、IP选项等）。它通常包含在 msghdr 结构体的 msg_control 字段中。
	//struct cmsghdr {
	//	socklen_t cmsg_len;   // 控制信息的长度
	//	int       cmsg_level; // 控制信息的协议级别
	//	int       cmsg_type;  // 控制信息的类型
	//	// 后面是控制信息的数据
	//};

	int SendFD(int fd) {//主进程完成
		struct msghdr msg; //消息头，用于传递消息，包括数据和控制信息
		iovec iov[2]; //iovec是一个结构体，用于描述一个数据缓冲区，包括缓冲区的地址和长度
		char buf[2][10] = { "edoyun","jueding" };
		iov[0].iov_base = buf[0]; //缓冲区的地址
		iov[0].iov_len = sizeof(buf[0]); //缓冲区的长度
		iov[1].iov_base = buf[1]; //缓冲区的地址
		iov[1].iov_len = sizeof(buf[1]); //缓冲区的长度
		msg.msg_iov = iov; //iov数组的地址，指向一个iovec数组
		msg.msg_iovlen = 2; //iov数组的长度

		// 下面的数据，才是我们需要传递的。
		cmsghdr* cmsg = (cmsghdr*)calloc(1, CMSG_LEN(sizeof(int))); //cmsghdr是一个结构体，用于描述控制信息，如文件描述符传递、IP选项等
		if (cmsg == NULL)return -1;
		cmsg->cmsg_len = CMSG_LEN(sizeof(int)); //控制信息的长度
		cmsg->cmsg_level = SOL_SOCKET; //控制信息的协议级别
		cmsg->cmsg_type = SCM_RIGHTS;	//控制信息的类型
		*(int*)CMSG_DATA(cmsg) = fd; //控制信息的数据,这里是文件描述符
		msg.msg_control = cmsg; //控制信息的地址
		msg.msg_controllen = cmsg->cmsg_len; //控制信息的长度

		ssize_t ret = sendmsg(pipes[1], &msg, 0); //发送消息
		free(cmsg); //释放控制信息的内存
		if (ret == -1) {
			return -2;
		}
		return 0;
	}

	int RecvFD(int& fd) //子进程完成,接收文件描述符
	{
		msghdr msg; //消息头
		iovec iov[2]; //iovec数组
		char buf[][10] = { "","" }; //缓冲区
		iov[0].iov_base = buf[0]; //缓冲区的地址
		iov[0].iov_len = sizeof(buf[0]);	//缓冲区的长度
		iov[1].iov_base = buf[1]; //缓冲区的地址
		iov[1].iov_len = sizeof(buf[1]); //缓冲区的长度
		msg.msg_iov = iov; //iovec数组的地址
		msg.msg_iovlen = 2; //iovec数组的长度

		cmsghdr* cmsg = (cmsghdr*)calloc(1, CMSG_LEN(sizeof(int))); //控制信息
		if (cmsg == NULL)return -1;
		cmsg->cmsg_len = CMSG_LEN(sizeof(int)); //控制信息的长度
		cmsg->cmsg_level = SOL_SOCKET; //控制信息的协议级别
		cmsg->cmsg_type = SCM_RIGHTS; //控制信息的类型
		msg.msg_control = cmsg; //控制信息的地址
		msg.msg_controllen = CMSG_LEN(sizeof(int)); //控制信息的长度
		ssize_t ret = recvmsg(pipes[0], &msg, 0); //接收消息
		if (ret == -1) {
			free(cmsg);
			return -2;
		}
		fd = *(int*)CMSG_DATA(cmsg); //获取文件描述符
		return 0;
	}
	/*守护进程通常会关闭标准输入输出（stdin、stdout、stderr），原因如下：
		1.	避免占用终端：
		•	守护进程在后台运行，不需要与用户交互，因此不需要标准输入输出。
		•	关闭标准输入输出可以避免占用终端资源。
		2.	防止意外输出：
		使用 umask 清除文件遮蔽位
		umask 是一个进程级别的设置，用于控制新创建文件的默认权限。守护进程通常会将 umask 设置为 0，原因如下：
		1.	确保文件权限正确：
		•	守护进程可能需要创建文件或目录，设置 umask 为 0 可以确保新创建的文件或目录具有预期的权限。
		•	这可以避免由于默认 umask 设置导致的权限问题。
		处理 SIGCHLD 信号
		SIGCHLD 信号是在子进程终止时发送给父进程的信号。守护进程通常需要处理 SIGCHLD 信号，原因如下：
		1.	避免僵尸进程：
		•	当子进程终止时，如果父进程没有处理 SIGCHLD 信号，子进程会变成僵尸进程，占用系统资源。
		•	通过处理 SIGCHLD 信号，父进程可以调用 wait 或 waitpid 函数回收子进程的资源，避免僵尸进程的产生。*/
	static int SwitchDeamon() {
		pid_t ret = fork();
		if (ret == -1)return -1;
		if (ret > 0)exit(0);//主进程到此为止
		//子进程内容如下
		ret = setsid();
		if (ret == -1)return -2;//失败，则返回
		ret = fork();
		if (ret == -1)return -3;
		if (ret > 0)exit(0);//子进程到此为止
		//孙进程的内容如下，进入守护状态
		for (int i = 0; i < 3; i++) close(i);//关闭标准输入，输出，错误输出
		umask(0); //设置文件权限掩码,清除文件遮蔽位
		signal(SIGCHLD, SIG_IGN);//忽略SIGCHLD信号
		return 0;
	}


private:
	CFunctionBase* m_func; //函数对象
	pid_t m_pid; //进程id
	int pipes[2];
};