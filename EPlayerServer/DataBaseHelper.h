#pragma once
#include "Public.h"
#include <map>
#include <list>
#include <memory>
#include <vector>

class _Table_;

using KeyValue = std::map<Buffer, Buffer>;//键值对
using Result = std::list<_Table_>;//结果集

class CDatabaseClient//数据库客户端基类
{
public:
	CDatabaseClient(const CDatabaseClient&) = delete;
	CDatabaseClient& operator=(const CDatabaseClient&) = delete;
public:
	CDatabaseClient() {}
	virtual ~CDatabaseClient() {}
public:
	//连接
	virtual int Connect(const KeyValue& args) = 0;
	//执行
	virtual int Exec(const Buffer& sql) = 0;
	//带结果的执行
	virtual int Exec(const Buffer& sql, Result& result, const _Table_& table) = 0;
	//开启事务
	virtual int StartTransaction() = 0;
	//提交事务
	virtual int CommitTransaction() = 0;
	//回滚事务
	virtual int RollbackTransaction() = 0;
	//关闭连接
	virtual int Close() = 0;
	//是否连接
	virtual bool IsConnected() = 0;
};

//表和列的基类的实现
class _Field_;

using PField = std::shared_ptr<_Field_>;//列字段的智能指针
using FieldArray = std::vector<PField>; //列字段的数组
using FieldMap = std::map<Buffer, PField>; //列字段的映射表

class _Table_;
using PTable = std::shared_ptr<_Table_>;
class _Table_ {
public:
	_Table_() {}
	virtual ~_Table_() {}
	//返回创建的SQL语句
	virtual Buffer Create() = 0;
	//删除表
	virtual Buffer Drop() = 0;
	//增删改查
	//TODO:参数进行优化
	virtual Buffer Insert(const _Table_& values) = 0;
	virtual Buffer Delete() = 0;
	//TODO:参数进行优化
	virtual Buffer Modify() = 0;
	virtual Buffer Query() = 0;
	//创建一个基于表的对象
	virtual PTable Copy() = 0;
public:
	//获取表的全名
	virtual operator const Buffer() const = 0;
public:
	//表所属的DB的名称
	Buffer Database;
	Buffer Name;
	FieldArray FieldDefine;//列的定义（存储查询结果）
	FieldMap Fields;//列的定义映射表
};

class _Field_//字段的基类
{
public:
	_Field_() {}
	_Field_(const _Field_& field) { //拷贝构造函数
		Name = field.Name;
		Type = field.Type;
		Attr = field.Attr;
		Default = field.Default;
		Check = field.Check;
	}
	virtual _Field_& operator=(const _Field_& field) { //赋值运算符
		if (this != &field) {
			Name = field.Name;
			Type = field.Type;
			Attr = field.Attr;
			Default = field.Default;
			Check = field.Check;
		}
		return *this;
	}
	virtual ~_Field_() {}
public:
	virtual Buffer Create() = 0;
	virtual void LoadFromStr(const Buffer& str) = 0;
	//where 语句使用的
	virtual Buffer toEqualExp() const = 0;
	virtual Buffer toSqlStr() const = 0;
	//列的全名
	virtual operator const Buffer() const = 0;
public:
	Buffer Name;//列的名称
	Buffer Type; //列的类型
	Buffer Size; //列的大小
	unsigned Attr; //列的属性
	Buffer Default; //列的默认值
	Buffer Check; //列的检查约束
};