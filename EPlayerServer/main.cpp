#include <cstdio>

#include <unistd.h>
#include <sys/types.h>
#include <functional>
#include <memory.h>
#include <sys/socket.h>‘
#include<sys/stat.h>
#include<fcntl.h>


class CFunctionBase
{
public:
	virtual ~CFunctionBase() {}
	virtual int operator()() = 0;
};

template<typename _FUNCTION_, typename... _ARGS_>
class CFunction :public CFunctionBase
{
public:
	CFunction(_FUNCTION_ func, _ARGS_... args)
		:m_binder(std::forward<_FUNCTION_>(func), std::forward<_ARGS_>(args)...) //这是一个绑定器，绑定了函数和参数
	{}//forward是一个完美转发函数，可以保持参数的引用类型
	virtual ~CFunction() {}
	virtual int operator()() {
		return m_binder();
	}
	typename std::_Bindres_helper<int, _FUNCTION_, _ARGS_...>::type m_binder; //这是一个绑定器，绑定了函数和参数
};

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


private:
	CFunctionBase* m_func; //函数对象
	pid_t m_pid; //进程id
	int pipes[2];
};


int CreateLogServer(CProcess* proc)
{
	printf("%s(%d):<%s> pid=%d\n", __FILE__, __LINE__, __FUNCTION__, getpid());
	return 0;
}

int CreateClientServer(CProcess* proc)
{
	printf("%s(%d):<%s> pid=%d\n", __FILE__, __LINE__, __FUNCTION__, getpid());
	int fd = -1;
	int ret = proc->RecvFD(fd);
	printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
	printf("%s(%d):<%s> fd=%d\n", __FILE__, __LINE__, __FUNCTION__, fd);
	sleep(1);
	char buf[10] = "";
	lseek(fd, 0, SEEK_SET);
	read(fd, buf, sizeof(buf));
	printf("%s(%d):<%s> buf=%s\n", __FILE__, __LINE__, __FUNCTION__, buf);
	close(fd);
	return 0;
}

int main()
{
	CProcess proclog, proccliets; //创建两个进程对象 
	printf("%s(%d):<%s> pid=%d\n", __FILE__, __LINE__, __FUNCTION__, getpid());
	proclog.SetEntryFunction(CreateLogServer, &proclog); //设置进程的入口函数
	int ret = proclog.CreateSubProcess(); //创建日志子进程
	if (ret != 0) {
		printf("%s(%d):<%s> pid=%d\n", __FILE__, __LINE__, __FUNCTION__, getpid());
		return -1;
	}
	printf("%s(%d):<%s> pid=%d\n", __FILE__, __LINE__, __FUNCTION__, getpid());
	proccliets.SetEntryFunction(CreateClientServer, &proccliets); //设置进程的入口函数
	ret = proccliets.CreateSubProcess(); //创建子进程，客户端进程
	if (ret != 0) {
		printf("%s(%d):<%s> pid=%d\n", __FILE__, __LINE__, __FUNCTION__, getpid());
		return -2;
	}
	printf("%s(%d):<%s> pid=%d\n", __FILE__, __LINE__, __FUNCTION__, getpid());
	usleep(100 * 000);
	int fd = open("./test.txt", O_RDWR | O_CREAT | O_APPEND);
	printf("%s(%d):<%s> fd=%d\n", __FILE__, __LINE__, __FUNCTION__, fd);
	if (fd == -1)return -3;
	ret = procclients.SendFD(fd);
	printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
	if (ret != 0)printf("errno:%d msg:%s\n", errno, strerror(errno));
	write(fd, "edoyun", 6);
	close(fd);
	return 0;
	return 0;
}