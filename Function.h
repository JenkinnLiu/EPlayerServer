#pragma once 
#include <cstdio>
#include <memory.h>
#include <functional>
#include <unistd.h>
#include <sys/types.h>
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
