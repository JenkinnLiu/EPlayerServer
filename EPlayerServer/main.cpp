#include "Process.h"



int CreateLogServer(CProcess* proc)
{
	printf("%s(%d):<%s> pid=%d\n", __FILE__, __LINE__, __FUNCTION__, getpid());
	return 0;
}

int CreateClientServer(CProcess* proc)
{
	printf("%s(%d):<%s> pid=%d\n", __FILE__, __LINE__, __FUNCTION__, getpid());
	int fd = -1;
	int ret = proc->RecvFD(fd); //接收文件描述符
	printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret); //打印返回值
	printf("%s(%d):<%s> fd=%d\n", __FILE__, __LINE__, __FUNCTION__, fd); //打印文件描述符
	sleep(1); //睡眠1秒,等待主进程写文件
	char buf[10] = "";
	lseek(fd, 0, SEEK_SET); //文件指针移动到文件开头
	read(fd, buf, sizeof(buf)); //读取文件内容
	printf("%s(%d):<%s> buf=%s\n", __FILE__, __LINE__, __FUNCTION__, buf); //打印文件内容
	close(fd);
	return 0;
}

int main()
{
	//CProcess::SwitchDeamon(); //创建守护进程
	CProcess proclog, proccliets; //创建两个进程对象 
	printf("%s(%d):<%s> pid=%d\n", __FILE__, __LINE__, __FUNCTION__, getpid()); //打印进程id
	proclog.SetEntryFunction(CreateLogServer, &proclog); //设置进程的入口函数
	int ret = proclog.CreateSubProcess(); //创建日志子进程
	if (ret != 0) {
		printf("%s(%d):<%s> pid=%d\n", __FILE__, __LINE__, __FUNCTION__, getpid()); //打印进程id
		return -1;
	}
	printf("%s(%d):<%s> pid=%d\n", __FILE__, __LINE__, __FUNCTION__, getpid()); //打印进程id
	proccliets.SetEntryFunction(CreateClientServer, &proccliets); //设置进程的入口函数
	ret = proccliets.CreateSubProcess(); //创建子进程，客户端进程
	if (ret != 0) {
		printf("%s(%d):<%s> pid=%d\n", __FILE__, __LINE__, __FUNCTION__, getpid()); //打印进程id
		return -2;
	}
	printf("%s(%d):<%s> pid=%d\n", __FILE__, __LINE__, __FUNCTION__, getpid()); //打印进程id
	usleep(100 * 000); //睡眠100毫秒，等待子进程创建完成，usleep是微秒级别的睡眠函数
	int fd = open("./test.txt", O_RDWR | O_CREAT | O_APPEND); //打开文件
	printf("%s(%d):<%s> fd=%d\n", __FILE__, __LINE__, __FUNCTION__, fd); //打印文件描述符
	if (fd == -1)return -3;
	ret = proccliets.SendFD(fd); //发送文件描述符
	printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret); //打印返回值
	if (ret != 0)printf("errno:%d msg:%s\n", errno, strerror(errno)); //打印错误信息
	write(fd, "edoyun", 6); //写文件
	close(fd);
	return 0;
}