#pragma once
#include "Public.h"
#include "DatabaseHelper.h"
#include "sqlite3/sqlite3.h"

class CSqlite3Client//sqlite3客户端类 
	:public CDatabaseClient
{
public:
	CSqlite3Client(const CSqlite3Client&) = delete;
	CSqlite3Client& operator=(const CSqlite3Client&) = delete;
public:
	CSqlite3Client() {
		m_db = NULL;
		m_stmt = NULL;
	}
	virtual ~CSqlite3Client() {
		Close();
	}
public:
	//连接
	virtual int Connect(const KeyValue& args);
	//执行
	virtual int Exec(const Buffer& sql);
	//带结果的执行,结果集存放在result中
	virtual int Exec(const Buffer& sql, Result& result, const _Table_& table);
	//开启事务，事务是一组操作，要么全部成功，要么全部失败
	virtual int StartTransaction();
	//提交事务
	virtual int CommitTransaction();
	//回滚事务，即撤销事务
	virtual int RollbackTransaction();
	//关闭连接
	virtual int Close();
	//是否连接 true表示连接中 false表示未连接
	virtual bool IsConnected();
private:
	static int ExecCallback(void* arg, int count, char** names, char** values);//执行回调函数
	int ExecCallback(Result& result, const _Table_& table, int count, char** names, char** values);
private:
	sqlite3_stmt* m_stmt;//sqlite3语句对象
	sqlite3* m_db;//sqlite3数据库对象
private:
	//ExecParam 类的主要作用是封装执行 SQL 语句时所需的参数，并在回调函数中传递这些参数。
	class ExecParam {//执行参数的内部类
	public:
		ExecParam(CSqlite3Client* obj, Result& result, const _Table_& table)//构造函数
			:obj(obj), result(result), table(table)
		{}
		CSqlite3Client* obj;//sqlite3客户端对象
		Result& result;//结果集
		const _Table_& table;//表对象
	};
};

class _sqlite3_table_ : //sqlite3表类
	public _Table_
{
public:
	_sqlite3_table_() :_Table_() {} //构造函数
	_sqlite3_table_(const _sqlite3_table_& table); //拷贝构造函数
	virtual ~_sqlite3_table_();
	//返回创建的SQL语句
	virtual Buffer Create(); //创建表
	//删除表
	virtual Buffer Drop(); //删除表
	//增删改查
	//TODO:参数进行优化
	virtual Buffer Insert(const _Table_& values);//插入
	virtual Buffer Delete(const _Table_& values); //删除
	//TODO:参数进行优化
	virtual Buffer Modify(const _Table_& values); //修改
	virtual Buffer Query(); //查询
	//创建一个基于表的对象
	virtual PTable Copy()const; //拷贝
	virtual void ClearFieldUsed(); //清除字段使用
public:
	//获取表的全名
	virtual operator const Buffer() const;
};

class _sqlite3_field_ :
	public _Field_
{
public:
	_sqlite3_field_(); //构造函数
	_sqlite3_field_(
		int ntype,
		const Buffer& name,
		unsigned attr,
		const Buffer& type,
		const Buffer& size,
		const Buffer& default_,
		const Buffer& check
	);
	_sqlite3_field_(const _sqlite3_field_& field);//拷贝构造函数
	virtual ~_sqlite3_field_();
	virtual Buffer Create(); //创建
	virtual void LoadFromStr(const Buffer& str); //从字符串中加载
	//where 语句使用的
	virtual Buffer toEqualExp() const;//转化成等于表达式
	virtual Buffer toSqlStr() const;//转化成SQL字符串
	//列的全名
	virtual operator const Buffer() const;
private:
	Buffer Str2Hex(const Buffer& data) const;//字符串转换成16进制
	union {
		bool Bool;
		int Integer;
		double Double;
		Buffer* String;
	}Value;
	int nType;
};

