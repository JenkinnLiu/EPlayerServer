#include "Process.h"
#include "Logger.h"


int CreateLogServer(CProcess* proc)
{
	//printf("%s(%d):<%s> pid=%d\n", __FILE__, __LINE__, __FUNCTION__, getpid());
	CLoggerServer server; // 初始化日志服务器
	int ret = server.Start(); // 启动日志服务器
	if (ret != 0) {
		printf("%s(%d):<%s> pid=%d errno:%d msg:%s ret:%d\n",
			__FILE__, __LINE__, __FUNCTION__, getpid(), errno, strerror(errno), ret);
	}
	int fd = 0;
	while (true) { //循环接收文件描述符
		ret = proc->RecvFD(fd); 
		printf("%s(%d):<%s> fd=%d\n", __FILE__, __LINE__, __FUNCTION__, fd);
		if (fd <= 0)break;
	}
	ret = server.Close(); //关闭日志服务器
	printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
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

int LogTest()
{
	char buffer[] = "hello edoyun! 刘健强";
	usleep(1000 * 100);
	TRACEI("here is log %d %c %f %g %s 哈哈 嘻嘻 刘健强测试", 10, 'A', 1.0f, 2.0, buffer);
	DUMPD((void*)buffer, (size_t)sizeof(buffer));
	LOGE << 100 << " " << 'S' << " " << 0.12345f << " " << 1.23456789 << " " << buffer << " 易道云编程";
	return 0;
}

int old_test()
{
	//CProcess::SwitchDeamon();
	CProcess proclog, procclients;
	printf("%s(%d):<%s> pid=%d\n", __FILE__, __LINE__, __FUNCTION__, getpid());
	proclog.SetEntryFunction(CreateLogServer, &proclog);
	int ret = proclog.CreateSubProcess();
	if (ret != 0) {
		printf("%s(%d):<%s> pid=%d\n", __FILE__, __LINE__, __FUNCTION__, getpid());
		return -1;
	}
	LogTest();
	printf("%s(%d):<%s> pid=%d\n", __FILE__, __LINE__, __FUNCTION__, getpid());
	CThread thread(LogTest);
	thread.Start();
	procclients.SetEntryFunction(CreateClientServer, &procclients);
	ret = procclients.CreateSubProcess();
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
	CThreadPool pool;
	ret = pool.Start(4);
	printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
	if (ret != 0)printf("errno:%d msg:%s\n", errno, strerror(errno));
	ret = pool.AddTask(LogTest);
	printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
	if (ret != 0)printf("errno:%d msg:%s\n", errno, strerror(errno));
	ret = pool.AddTask(LogTest);
	printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
	if (ret != 0)printf("errno:%d msg:%s\n", errno, strerror(errno));
	ret = pool.AddTask(LogTest);
	printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
	if (ret != 0)printf("errno:%d msg:%s\n", errno, strerror(errno));
	ret = pool.AddTask(LogTest);
	printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
	if (ret != 0)printf("errno:%d msg:%s\n", errno, strerror(errno));
	(void)getchar();
	pool.Close();
	proclog.SendFD(-1);
	(void)getchar();
	return 0;
}

int main()
{
	int ret = 0;
	CProcess proclog;
	ret = proclog.SetEntryFunction(CreateLogServer, &proclog);
	ERR_RETURN(ret, -1);
	ret = proclog.CreateSubProcess();
	ERR_RETURN(ret, -2);
	CEdoyunPlayerServer business(2);
	CServer server;
	ret = server.Init(&business);
	ERR_RETURN(ret, -3);
	ret = server.Run();
	ERR_RETURN(ret, -4);
	return 0;
}