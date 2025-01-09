//Git
#include <cstdio>

#include <unistd.h>
#include <functional>

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
	CFunction(_FUNCTION_ func, _ARGS_... args) {
	}
	virtual ~CFunction() {}
	virtual int operator()() {
		return m_binder();
	}
	std::_Bindres_helper<int, _FUNCTION_, _ARGS_...>::type m_binder;
};

class CProcess
{
public:
	CProcess() {
		m_func = NULL;
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
		m_func = new CFunction(func, args...);
		return 0;
	}

	int CreateSubProcess() {
		if (m_func == NULL)return -1;
		pid_t pid = fork();
		if (pid == -1)return -2;
		if (pid == 0) {
			//子进程
			return (*m_func)();
		}
		//主进程
		m_pid = pid;
		return 0;
	}

private:
	CFunctionBase* m_func;
	pid_t m_pid;
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
	CProcess proclog, proccliets;
	proclog.SetEntryFunction(CreateLogServer, &proclog);
	int ret = proclog.CreateSubProcess();
	proccliets.SetEntryFunction(CreateClientServer, &proccliets);
	ret = proccliets.CreateSubProcess();
	return 0;
}