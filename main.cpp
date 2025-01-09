#include <cstdio>

#include <unistd.h>
#include <functional>

//模板类的类型在执行的时候只有进入Entry函数时才能确认，CFunction构造的时候确认不了参数类型，所以
//这里要设置一个CFunctionBase基类，然后在CFunction类中继承CFunctionBase基类，这样就可以在CFunction类中
class CFunctionBase // 函数基类
{
public:
	virtual ~CFunctionBase() {} // 虚析构函数，防止子类析构不干净
	virtual int operator()() = 0; //第一个括号代表重载()运算符，第二个括号代表函数调用
};

template<typename _FUNCTION_, typename... _ARGS_> // 函数模板， ...代表可变参数，_FUNCTION_代表函数类型，_ARGS_代表参数类型
class CFunction :public CFunctionBase
{
public:
	CFunction(_FUNCTION_ func, _ARGS_... args) {
	} // 构造函数
	virtual ~CFunction() {} // 虚析构函数
	virtual int operator()() { // 重载()运算符
		return m_binder(); // 返回m_binder的值
	}
	std::_Bindres_helper<int, _FUNCTION_, _ARGS_...>::type m_binder;//在构造函数中，m_binder应该被初始化为一个绑定器对象，
	//这个对象可以在调用时执行指定的函数并传递相应的参数。

class CProcess // 进程类
{
public:
	CProcess() {
		m_func = NULL;
	}
	~CProcess() {
		if (m_func != NULL) {
			delete m_func;
			m_func = NULL;
		} // 析构函数
	}

	template<typename _FUNCTION_, typename... _ARGS_> // 函数模板， ...代表可变参数，_FUNCTION_代表函数类型，_ARGS_代表参数类型
	int SetEntryFunction(_FUNCTION_ func, _ARGS_... args) //进程入口函数，第一个参数为调用的函数，第二个参数为进程
	{
		m_func = new CFunction(func, args...); // 创建一个新的CFunction对象
		return 0;
	}

	int CreateSubProcess() { // 创建子进程
		if (m_func == NULL)return -1;
		pid_t pid = fork();
		if (pid == -1)return -2; // 创建子进程失败
		if (pid == 0) {
			//子进程 
			return (*m_func)(); // 调用m_func函数
		}
		//主进程
		m_pid = pid; // 记录子进程的pid
		return 0;
	}

private:
	CFunctionBase* m_func; // 函数指针
	pid_t m_pid; // 进程id
};


int CreateLogServer(CProcess* proc)
{
	return 0;
}

int CreateClientServer(CProcess* proc)
{
	return 0;
}

int main()
{
	CProcess proclog, proccliets; // 创建两个进程对象
	proclog.SetEntryFunction(CreateLogServer, &proclog); // 设置进程入口函数，在进程proclog中调用CreateLogServer函数
	int ret = proclog.CreateSubProcess(); // 在proclog进程下创建子进程
	proccliets.SetEntryFunction(CreateClientServer, &proccliets); // 设置进程入口函数，在进程proccliets中调用CreateClientServer函数
	ret = proccliets.CreateSubProcess(); // 在proccliets进程下创建子进程
	return 0;
}