#include "Sqlite3Client.h"
#include "Logger.h"

int CSqlite3Client::Connect(const KeyValue& args)//连接
{
	auto it = args.find("host"); //查找host
	if (it == args.end())return -1;
	if (m_db != NULL)return -2;
	int ret = sqlite3_open(it->second, &m_db);//调用sqlite3_open函数打开数据库
	if (ret != 0) {
		TRACEE("connect failed:%d [%s]", ret, sqlite3_errmsg(m_db));
		return -3;
	}
	return 0;
}

int CSqlite3Client::Exec(const Buffer& sql)//执行Sql语句
{
	printf("sql={%s}\n", (char*)sql);
	if (m_db == NULL)return -1;
	int ret = sqlite3_exec(m_db, sql, NULL, this, NULL);//调用sqlite3_exec函数执行sql语句
	if (ret != SQLITE_OK) {
		printf("sql={%s}\n", (char*)sql);
		printf("Exec failed:%d [%s]\n", ret, sqlite3_errmsg(m_db));
		return -2;
	}
	return 0;
}

int CSqlite3Client::Exec(const Buffer& sql, Result& result, const _Table_& table)//返回结果集的执行
{
	char* errmsg = NULL;
	if (m_db == NULL)return -1;
	printf("sql={%s}\n", (char*)sql);
	ExecParam param(this, result, table);
	int ret = sqlite3_exec(m_db, sql,
		&CSqlite3Client::ExecCallback, (void*)&param, &errmsg);//调用sqlite3_exec函数执行sql语句
	if (ret != SQLITE_OK) {
		printf("sql={%s}\n", sql);
		printf("Exec failed:%d [%s]\n", ret, errmsg);
		if (errmsg)sqlite3_free(errmsg);//执行失败，释放errmsg
		return -2;
	}
	if (errmsg)sqlite3_free(errmsg);
	return 0;
}

int CSqlite3Client::StartTransaction()
{
	if (m_db == NULL)return -1;
	int ret = sqlite3_exec(m_db, "BEGIN TRANSACTION", 0, 0, NULL);//调用sqlite3_exec函数初始化事务
	if (ret != SQLITE_OK) {
		TRACEE("sql={BEGIN TRANSACTION}");
		TRACEE("BEGIN failed:%d [%s]", ret, sqlite3_errmsg(m_db));
		return -2;
	}
	return 0;
}

int CSqlite3Client::CommitTransaction()//提交事务
{
	if (m_db == NULL)return -1;
	int ret = sqlite3_exec(m_db, "COMMIT TRANSACTION", 0, 0, NULL);//调用sqlite3_exec函数执行sql语句，提交事务
	if (ret != SQLITE_OK) {
		TRACEE("sql={COMMIT TRANSACTION}");
		TRACEE("COMMIT failed:%d [%s]", ret, sqlite3_errmsg(m_db));
		return -2;
	}
	return 0;
}

int CSqlite3Client::RollbackTransaction()//回滚事务
{
	if (m_db == NULL)return -1;
	int ret = sqlite3_exec(m_db, "ROLLBACK TRANSACTION", 0, 0, NULL);//调用sqlite3_exec函数回滚事务
	if (ret != SQLITE_OK) {
		TRACEE("sql={ROLLBACK TRANSACTION}");
		TRACEE("ROLLBACK failed:%d [%s]", ret, sqlite3_errmsg(m_db));
		return -2;
	}
	return 0;
}

int CSqlite3Client::Close()//关闭连接
{
	if (m_db == NULL)return -1;
	int ret = sqlite3_close(m_db);//调用sqlite3_close函数关闭数据库
	if (ret != SQLITE_OK) {
		TRACEE("Close failed:%d [%s]", ret, sqlite3_errmsg(m_db));
		return -2;
	}
	m_db = NULL;
	return 0;
}

bool CSqlite3Client::IsConnected()//检查是否连接
{
	return m_db != NULL;
}
//执行回调函数
int CSqlite3Client::ExecCallback(void* arg, int count, char** values, char** names)//执行回调函数
{
	ExecParam* param = (ExecParam*)arg;
	return param->obj->ExecCallback(param->result, param->table, count, names, values);
}
//返回结果集的回调函数
int CSqlite3Client::ExecCallback(Result& result, const _Table_& table, int count, char** names, char** values)
{
	PTable pTable = table.Copy();//拷贝表
	if (pTable == nullptr) {
		printf("table %s error!\n", (const char*)(Buffer)table);
		return -1;
	}
	for (int i = 0; i < count; i++) {//遍历所有列字段，将字段值赋给表
		Buffer name = names[i];
		auto it = pTable->Fields.find(name);
		if (it == pTable->Fields.end()) {
			printf("table %s error!\n", (const char*)(Buffer)table);
			return -2;
		}
		if (values[i] != NULL)
			it->second->LoadFromStr(values[i]);
	}
	result.push_back(pTable);
	return 0;
}

_sqlite3_table_::_sqlite3_table_(const _sqlite3_table_& table)//拷贝构造函数
	:_Table_(table)
{
	Database = table.Database;
	Name = table.Name;
	for (size_t i = 0; i < table.FieldDefine.size(); i++)//遍历所有字段定义，将字段定义赋给表
	{
		PField field = PField(new _sqlite3_field_(*
			(_sqlite3_field_*)table.FieldDefine[i].get()));
		FieldDefine.push_back(field);
		Fields[field->Name] = field;
	}
}

_sqlite3_table_::~_sqlite3_table_()
{}

Buffer _sqlite3_table_::Create()
{	//CREATE TABLE IF NOT EXISTS 表全名 (列定义,……);
	//表全名 = 数据库.表名
	//(Buffer)*this = 表全名
	Buffer sql = "CREATE TABLE IF NOT EXISTS " + (Buffer)*this + "(\r\n";
	for (size_t i = 0; i < FieldDefine.size(); i++) {
		if (i > 0)sql += ",";
		sql += FieldDefine[i]->Create();//添加所有列定义
	}
	sql += ");";
	TRACEI("sql = %s", (char*)sql);
	return sql;
}

Buffer _sqlite3_table_::Drop()//删除表
{
	Buffer sql = "DROP TABLE " + (Buffer)*this + ";";
	TRACEI("sql = %s", (char*)sql);
	return sql;
}

Buffer _sqlite3_table_::Insert(const _Table_& values)//插入
{	//INSERT INTO 表全名 (列1,...,列n)
	//VALUES(值1,...,值n);
	Buffer sql = "INSERT INTO " + (Buffer)*this + " (";
	bool isfirst = true;
	for (size_t i = 0; i < values.FieldDefine.size(); i++) {//遍历所有列字段
		if (values.FieldDefine[i]->Condition & SQL_INSERT) {
			if (!isfirst)sql += ",";
			else isfirst = false;
			sql += (Buffer)*values.FieldDefine[i];
		}
	}
	sql += ") VALUES (";
	isfirst = true;
	for (size_t i = 0; i < values.FieldDefine.size(); i++) {//遍历所有值
		if (values.FieldDefine[i]->Condition & SQL_INSERT) {
			if (!isfirst)sql += ",";
			else isfirst = false;
			sql += values.FieldDefine[i]->toSqlStr();
		}
	}
	sql += " );";
	TRACEI("sql = %s", (char*)sql);
	return sql;
}

Buffer _sqlite3_table_::Delete(const _Table_& values)
{// DELETE FROM 表全名 WHERE 条件
	Buffer sql = "DELETE FROM " + (Buffer)*this + " ";
	Buffer Where = "";
	bool isfirst = true;
	for (size_t i = 0; i < FieldDefine.size(); i++) {
		if (FieldDefine[i]->Condition & SQL_CONDITION) {//遍历所有条件
			if (!isfirst)Where += " AND ";
			else isfirst = false;
			Where += (Buffer)*FieldDefine[i] + "=" + FieldDefine[i]->toSqlStr();
		}
	}
	if (Where.size() > 0)
		sql += " WHERE " + Where;
	sql += ";";
	TRACEI("sql = %s", (char*)sql);
	return sql;
}

Buffer _sqlite3_table_::Modify(const _Table_& values)//修改
{
	//UPDATE 表全名 SET 列1=值1 , ... , 列n=值n [WHERE 条件];
	Buffer sql = "UPDATE " + (Buffer)*this + " SET ";
	bool isfirst = true;
	for (size_t i = 0; i < values.FieldDefine.size(); i++) {
		if (values.FieldDefine[i]->Condition & SQL_MODIFY) {
			if (!isfirst)sql += ",";
			else isfirst = false;
			sql += (Buffer)*values.FieldDefine[i] + "=" + values.FieldDefine[i]->toSqlStr();
		}
	}

	Buffer Where = "";
	for (size_t i = 0; i < values.FieldDefine.size(); i++) {
		if (values.FieldDefine[i]->Condition & SQL_CONDITION) {
			if (!isfirst)Where += " AND ";
			else isfirst = false;
			Where += (Buffer)*values.FieldDefine[i] + "=" + values.FieldDefine[i]->toSqlStr();
		}
	}
	if (Where.size() > 0)
		sql += " WHERE " + Where;
	sql += " ;";
	TRACEI("sql = %s", (char*)sql);
	return sql;
}

Buffer _sqlite3_table_::Query()//查询
{//SELECT 列名1 ,列名2 ,... ,列名n FROM 表全名;
	Buffer sql = "SELECT ";
	for (size_t i = 0; i < FieldDefine.size(); i++)//遍历所有列字段
	{
		if (i > 0)sql += ',';
		sql += '"' + FieldDefine[i]->Name + "\" ";
	}
	sql += " FROM " + (Buffer)*this + ";";
	TRACEI("sql = %s", (char*)sql);
	return sql;
}

PTable _sqlite3_table_::Copy() const//拷贝
{
	return PTable(new _sqlite3_table_(*this));
}

void _sqlite3_table_::ClearFieldUsed()//清除字段使用
{
	for (size_t i = 0; i < FieldDefine.size(); i++) {
		FieldDefine[i]->Condition = 0;
	}
}

_sqlite3_table_::operator const Buffer() const
{
	Buffer Head;
	if (Database.size())
		Head = '"' + Database + "\".";
	return Head + '"' + Name + '"';
}

_sqlite3_field_::_sqlite3_field_()
	:_Field_() {
	nType = TYPE_NULL;
	Value.Double = 0.0;
}

_sqlite3_field_::_sqlite3_field_(int ntype, const Buffer& name, unsigned attr, const Buffer& type, const Buffer& size, const Buffer& default_, const Buffer& check)//构造函数
	:_Field_()
{
	nType = ntype;
	switch (ntype)
	{
	case TYPE_VARCHAR:
	case TYPE_TEXT:
	case TYPE_BLOB:
		Value.String = new Buffer();//如果类型是字符，则创建一个Buffer对象存储
		break;
	}

	Name = name; //名称
	Attr = attr; //属性
	Type = type;//类型
	Size = size;//大小
	Default = default_;
	Check = check;
}

_sqlite3_field_::_sqlite3_field_(const _sqlite3_field_& field)//拷贝构造函数
	:_Field_(field)
{
	nType = field.nType;
	switch (field.nType)
	{
	case TYPE_VARCHAR:
	case TYPE_TEXT:
	case TYPE_BLOB:
		Value.String = new Buffer();
		*Value.String = *field.Value.String;
		break;
	}

	Name = field.Name;
	Attr = field.Attr;
	Type = field.Type;
	Size = field.Size;
	Default = field.Default;
	Check = field.Check;
}

_sqlite3_field_::~_sqlite3_field_()//析构函数
{
	switch (nType)
	{
	case TYPE_VARCHAR:
	case TYPE_TEXT:
	case TYPE_BLOB:
		if (Value.String) {
			Buffer* p = Value.String;
			Value.String = NULL;
			delete p;//把Value.String指向的Buffer对象删除
		}
		break;
	}
}

Buffer _sqlite3_field_::Create()//创建的sql语句
{	//"名称" 类型 属性
	//enum{
	//	NOT_NULL = 1,
	//	DEFAULT = 2,
	//	UNIQUE = 4,
	//	PRIMARY_KEY = 8,
	//	CHECK = 16,
	//	AUTOINCREMENT = 32
	//};
	Buffer sql = '"' + Name + "\" " + Type + " ";
	if (Attr & NOT_NULL) {
		sql += " NOT NULL ";
	}
	if (Attr & DEFAULT) {
		sql += " DEFAULT " + Default + " ";
	}
	if (Attr & UNIQUE) {
		sql += " UNIQUE ";
	}
	if (Attr & PRIMARY_KEY) {
		sql += " PRIMARY KEY ";
	}
	if (Attr & CHECK) {
		sql += " CHECK( " + Check + ") ";
	}
	if (Attr & AUTOINCREMENT) {
		sql += " AUTOINCREMENT ";
	}
	return sql;
}

void _sqlite3_field_::LoadFromStr(const Buffer& str)//从字符串中加载,即将字符串转换成对应的类型
{
	switch (nType)//根据类型加载
	{
	case TYPE_NULL:
		break;
	case TYPE_BOOL:
	case TYPE_INT:
	case TYPE_DATETIME:
		Value.Integer = atoi(str);
		break;
	case TYPE_REAL:
		Value.Double = atof(str);
		break;
	case TYPE_VARCHAR:
	case TYPE_TEXT:
		*Value.String = str;
		break;
	case TYPE_BLOB:
		*Value.String = Str2Hex(str);
		break;
	default:
		TRACEW("type=%d", nType);
		break;
	}
}

Buffer _sqlite3_field_::toEqualExp() const//转化成等于表达式
{
	Buffer sql = (Buffer)*this + " = ";//列名 =
	std::stringstream ss;
	switch (nType)
	{
	case TYPE_NULL://如果是空，则在sql后加上NULL
		sql += " NULL ";
		break;
	case TYPE_BOOL:
	case TYPE_INT:
	case TYPE_DATETIME://如果是bool、int、datetime类型，则在sql后加上对应的值
		ss << Value.Integer;
		sql += ss.str() + " ";
		break;
	case TYPE_REAL://如果是real类型，则在sql后加上对应的值
		ss << Value.Double;
		sql += ss.str() + " ";
		break;
	case TYPE_VARCHAR:
	case TYPE_TEXT:
	case TYPE_BLOB: //如果是varchar、text、blob类型，则在sql后加上双引号和对应的值
		sql += '"' + *Value.String + "\" ";
		break;
	default:
		TRACEW("type=%d", nType);
		break;
	}
	return sql;
}

Buffer _sqlite3_field_::toSqlStr() const//转化成SQL字符串
{
	Buffer sql = "";
	std::stringstream ss;
	switch (nType)//根据类型转化成对应的字符串
	{
	case TYPE_NULL:
		sql += " NULL ";
		break;
	case TYPE_BOOL:
	case TYPE_INT:
	case TYPE_DATETIME:
		ss << Value.Integer;
		sql += ss.str() + " ";
		break;
	case TYPE_REAL:
		ss << Value.Double;
		sql += ss.str() + " ";
		break;
	case TYPE_VARCHAR:
	case TYPE_TEXT:
	case TYPE_BLOB:
		sql += '"' + *Value.String + "\" ";
		break;
	default:
		TRACEW("type=%d", nType);
		break;
	}
	return sql;
}

_sqlite3_field_::operator const Buffer() const
{
	return '"' + Name + '"';
}

Buffer _sqlite3_field_::Str2Hex(const Buffer& data) const//字符串转换成16进制
{
	const char* hex = "0123456789ABCDEF";
	std::stringstream ss;
	for (auto ch : data)
		ss << hex[(unsigned char)ch >> 4] << hex[(unsigned char)ch & 0xF];
	return ss.str();
}



