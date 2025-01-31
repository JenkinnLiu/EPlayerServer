#pragma once
#include "Public.h"
#include "DatabaseHelper.h"
#include "sqlite3/sqlite3.h"

class CSqlite3Client//sqlite3�ͻ����� 
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
	//����
	virtual int Connect(const KeyValue& args);
	//ִ��
	virtual int Exec(const Buffer& sql);
	//�������ִ��,����������result��
	virtual int Exec(const Buffer& sql, Result& result, const _Table_& table);
	//��������������һ�������Ҫôȫ���ɹ���Ҫôȫ��ʧ��
	virtual int StartTransaction();
	//�ύ����
	virtual int CommitTransaction();
	//�ع����񣬼���������
	virtual int RollbackTransaction();
	//�ر�����
	virtual int Close();
	//�Ƿ����� true��ʾ������ false��ʾδ����
	virtual bool IsConnected();
private:
	static int ExecCallback(void* arg, int count, char** names, char** values);//ִ�лص�����
	int ExecCallback(Result& result, const _Table_& table, int count, char** names, char** values);
private:
	sqlite3_stmt* m_stmt;//sqlite3������
	sqlite3* m_db;//sqlite3���ݿ����
private:
	//ExecParam �����Ҫ�����Ƿ�װִ�� SQL ���ʱ����Ĳ��������ڻص������д�����Щ������
	class ExecParam {//ִ�в������ڲ���
	public:
		ExecParam(CSqlite3Client* obj, Result& result, const _Table_& table)//���캯��
			:obj(obj), result(result), table(table)
		{}
		CSqlite3Client* obj;//sqlite3�ͻ��˶���
		Result& result;//�����
		const _Table_& table;//�����
	};
};

class _sqlite3_table_ : //sqlite3����
	public _Table_
{
public:
	_sqlite3_table_() :_Table_() {} //���캯��
	_sqlite3_table_(const _sqlite3_table_& table); //�������캯��
	virtual ~_sqlite3_table_();
	//���ش�����SQL���
	virtual Buffer Create(); //������
	//ɾ����
	virtual Buffer Drop(); //ɾ����
	//��ɾ�Ĳ�
	//TODO:���������Ż�
	virtual Buffer Insert(const _Table_& values);//����
	virtual Buffer Delete(const _Table_& values); //ɾ��
	//TODO:���������Ż�
	virtual Buffer Modify(const _Table_& values); //�޸�
	virtual Buffer Query(); //��ѯ
	//����һ�����ڱ�Ķ���
	virtual PTable Copy()const; //����
	virtual void ClearFieldUsed(); //����ֶ�ʹ��
public:
	//��ȡ���ȫ��
	virtual operator const Buffer() const;
};

class _sqlite3_field_ :
	public _Field_
{
public:
	_sqlite3_field_(); //���캯��
	_sqlite3_field_(
		int ntype,
		const Buffer& name,
		unsigned attr,
		const Buffer& type,
		const Buffer& size,
		const Buffer& default_,
		const Buffer& check
	);
	_sqlite3_field_(const _sqlite3_field_& field);//�������캯��
	virtual ~_sqlite3_field_();
	virtual Buffer Create(); //����
	virtual void LoadFromStr(const Buffer& str); //���ַ����м���
	//where ���ʹ�õ�
	virtual Buffer toEqualExp() const;//ת���ɵ��ڱ��ʽ
	virtual Buffer toSqlStr() const;//ת����SQL�ַ���
	//�е�ȫ��
	virtual operator const Buffer() const;
private:
	Buffer Str2Hex(const Buffer& data) const;//�ַ���ת����16����
	union {
		bool Bool;
		int Integer;
		double Double;
		Buffer* String;
	}Value;
	int nType;
};

