#pragma once
#include "Logger.h"
#include "CServer.h"
#include "HttpParser.h"
#include "Crypto.h"
#include "MysqlClient.h"
#include "jsoncpp/json.h"
#include <map>

/*
首先，用户提供了一个头文件EPlayerServer.h的代码，属于一个名为CEdoyunPlayerServer的类，
继承自CBusiness。
看起来这是一个服务器端的业务处理类，处理客户端的连接和HTTP请求，特别是登录验证部分。
我需要详细解释这段代码的各个部分。
首先，我注意到代码开始处有一些宏定义，比如DECLARE_TABLE_CLASS和DECLARE_MYSQL_FIELD，
这些应该用于定义数据库表结构。
接着是ERR_RETURN和WARN_CONTINUE宏，用于错误处理。然后，CEdoyunPlayerServer类的构造函数和
析构函数中初始化了一些成员变量，如epoll、线程池、数据库连接等。
在BusinessProcess方法中，首先创建了数据库连接，连接到MySQL数据库。然后设置连接回调和接收回调函数，
使用std::placeholders绑定到当前对象的成员函数。接着创建epoll实例和线程池，并添加任务到线程池。
之后进入循环，接收客户端socket连接，将新的客户端socket添加到epoll中，并触发连接回调。
接下来是Connected和Received回调函数。Connected函数处理新客户端连接，打印客户端地址信息。
Received函数处理接收到的数据，调用HttpParser解析HTTP请求，根据解析结果生成响应返回给客户端。
HttpParser方法解析HTTP请求，判断是GET还是POST方法。如果是GET请求，解析URL参数，处理登录逻辑。
查询数据库验证用户信息，计算MD5签名进行验证。验证成功返回0，否则返回错误码。
MakeResponse方法根据验证结果生成JSON格式的HTTP响应，设置HTTP头信息，如状态码、日期、
内容类型等，并将JSON数据作为响应体返回。
ThreadFunc是线程函数，使用epoll监听事件，处理可读事件，接收客户端数据，并调用接收回调处理数据。
*/

DECLARE_TABLE_CLASS(edoyunLogin_user_mysql, _mysql_table_)
DECLARE_MYSQL_FIELD(TYPE_INT, user_id, NOT_NULL | PRIMARY_KEY | AUTOINCREMENT, "INTEGER", "", "", "")
DECLARE_MYSQL_FIELD(TYPE_VARCHAR, user_qq, NOT_NULL, "VARCHAR", "(15)", "", "")  //QQ号
DECLARE_MYSQL_FIELD(TYPE_VARCHAR, user_phone, DEFAULT, "VARCHAR", "(11)", "'18888888888'", "")  //手机
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_name, NOT_NULL, "TEXT", "", "", "")    //姓名
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_nick, NOT_NULL, "TEXT", "", "", "")    //昵称
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_wechat, DEFAULT, "TEXT", "", "NULL", "")
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_wechat_id, DEFAULT, "TEXT", "", "NULL", "")
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_address, DEFAULT, "TEXT", "", "", "")
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_province, DEFAULT, "TEXT", "", "", "")
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_country, DEFAULT, "TEXT", "", "", "")
DECLARE_MYSQL_FIELD(TYPE_INT, user_age, DEFAULT | CHECK, "INTEGER", "", "18", "")
DECLARE_MYSQL_FIELD(TYPE_INT, user_male, DEFAULT, "BOOL", "", "1", "")
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_flags, DEFAULT, "TEXT", "", "0", "")
DECLARE_MYSQL_FIELD(TYPE_REAL, user_experience, DEFAULT, "REAL", "", "0.0", "")
DECLARE_MYSQL_FIELD(TYPE_INT, user_level, DEFAULT | CHECK, "INTEGER", "", "0", "")
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_class_priority, DEFAULT, "TEXT", "", "", "")
DECLARE_MYSQL_FIELD(TYPE_REAL, user_time_per_viewer, DEFAULT, "REAL", "", "", "")
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_career, NONE, "TEXT", "", "", "")
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_password, NOT_NULL, "TEXT", "", "", "")
DECLARE_MYSQL_FIELD(TYPE_INT, user_birthday, NONE, "DATETIME", "", "", "")
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_describe, NONE, "TEXT", "", "", "")
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_education, NONE, "TEXT", "", "", "")
DECLARE_MYSQL_FIELD(TYPE_INT, user_register_time, DEFAULT, "DATETIME", "", "LOCALTIME()", "")
DECLARE_TABLE_CLASS_EDN()

/*
* 1. 客户端的地址问题
* 2. 连接回调的参数问题
* 3. 接收回调的参数问题
*/
#define ERR_RETURN(ret, err) if(ret!=0){TRACEE("ret= %d errno = %d msg = [%s]", ret, errno, strerror(errno));return err;}

#define WARN_CONTINUE(ret) if(ret!=0){TRACEW("ret= %d errno = %d msg = [%s]", ret, errno, strerror(errno));continue;}

class CEdoyunPlayerServer :
	public CBusiness
{
//成员变量

//m_epoll: 用于管理I / O多路复用的Epoll实例。

//m_pool : 线程池处理并发任务。

//m_db : MySQL数据库客户端，执行查询操作。

//m_mapClients : 存储客户端Socket的映射
public:
	CEdoyunPlayerServer(unsigned count) :CBusiness() { // 构造函数
		m_count = count;
	}
	~CEdoyunPlayerServer() {// 析构函数
		if (m_db) {
			CDatabaseClient* db = m_db;
			m_db = NULL;
			db->Close();
			delete db;
		}
		m_epoll.Close();
		m_pool.Close();
		for (auto it : m_mapClients) {
			if (it.second) {
				delete it.second;
			}
		}
		m_mapClients.clear();
	}
	virtual int BusinessProcess(CProcess* proc) { // 业务处理
		using namespace std::placeholders;
		int ret = 0;
		m_db = new CMysqlClient(); // 创建数据库连接
		if (m_db == NULL) {
			TRACEE("no more memory!");
			return -1;
		}
		KeyValue args;
		args["host"] = "192.168.31.76"; // 主机IP地址
		args["user"] = "root";
		args["password"] = "123456";
		args["port"] = 3306;// 端口
		args["db"] = "edoyun";
		ret = m_db->Connect(args);//连接数据库
		ERR_RETURN(ret, -2);
		edoyunLogin_user_mysql user;
		ret = m_db->Exec(user.Create());
		ERR_RETURN(ret, -3);
		ret = setConnectedCallback(&CEdoyunPlayerServer::Connected, this, _1);
		ERR_RETURN(ret, -4);
		ret = setRecvCallback(&CEdoyunPlayerServer::Received, this, _1, _2);
		ERR_RETURN(ret, -5);
		ret = m_epoll.Create(m_count);//创建epoll
		ERR_RETURN(ret, -6);
		ret = m_pool.Start(m_count);//启动线程池
		ERR_RETURN(ret, -7);
		for (unsigned i = 0; i < m_count; i++) {
			ret = m_pool.AddTask(&CEdoyunPlayerServer::ThreadFunc, this);//添加任务
			ERR_RETURN(ret, -8);
		}
		int sock = 0;
		sockaddr_in addrin;
		while (m_epoll != -1) {
			ret = proc->RecvSocket(sock, &addrin);//接收客户端套接字连接
			TRACEI("RecvSocket ret=%d", ret);
			if (ret < 0 || (sock == 0))break;
			CSocketBase* pClient = new CSocket(sock);//创建客户端套接字
			if (pClient == NULL)continue;
			ret = pClient->Init(CSockParam(&addrin, SOCK_ISIP));
			WARN_CONTINUE(ret);
			ret = m_epoll.Add(sock, EpollData((void*)pClient));
			if (m_connectedcallback) {
				(*m_connectedcallback)(pClient);
			}
			WARN_CONTINUE(ret);
		}
		return 0;
	}
private:
	int Connected(CSocketBase* pClient) {
		//TODO:客户端连接处理 简单打印一下客户端信息
		sockaddr_in* paddr = *pClient;
		TRACEI("client connected addr %s port:%d", inet_ntoa(paddr->sin_addr), paddr->sin_port);
		return 0;
	}

	int Received(CSocketBase* pClient, const Buffer& data) {
		TRACEI("接收到数据！");
		//TODO:主要业务，在此处理
		//HTTP 解析
		int ret = 0;
		Buffer response = "";
		ret = HttpParser(data);//解析HTTP请求
		TRACEI("HttpParser ret=%d", ret);
		//验证结果的反馈
		if (ret != 0) {//验证失败
			TRACEE("http parser failed!%d", ret);
		}
		response = MakeResponse(ret);
		ret = pClient->Send(response);
		if (ret != 0) {
			TRACEE("http response failed!%d [%s]", ret, (char*)response);
		}
		else {
			TRACEI("http response success!%d", ret);
		}
		return 0;
	}
	int HttpParser(const Buffer& data) {
		CHttpParser parser;
		size_t size = parser.Parser(data);
		if (size == 0 || (parser.Errno() != 0)) {
			TRACEE("size %llu errno:%u", size, parser.Errno());
			return -1;
		}
		if (parser.Method() == HTTP_GET) {
			//get 处理
			UrlParser url("https://192.168.31.76" + parser.Url());
			int ret = url.Parser();
			if (ret != 0) {
				TRACEE("ret = %d url[%s]", ret, "https://192.168.31.76" + parser.Url());
				return -2;
			}
			Buffer uri = url.Uri();
			TRACEI("**** uri = %s", (char*)uri);
			if (uri == "login") {
				//处理登录
				Buffer time = url["time"];//时间戳
				Buffer salt = url["salt"];//盐，盐是服务器端生成的，客户端不知道，用于加密
				Buffer user = url["user"];
				Buffer sign = url["sign"];//签名
				TRACEI("time %s salt %s user %s sign %s", (char*)time, (char*)salt, (char*)user, (char*)sign);
				//数据库的查询
				edoyunLogin_user_mysql dbuser;
				Result result;
				Buffer sql = dbuser.Query("user_name=\"" + user + "\"");//查询条件
				ret = m_db->Exec(sql, result, dbuser);//查询用户
				if (ret != 0) {
					TRACEE("sql=%s ret=%d", (char*)sql, ret);
					return -3;
				}
				if (result.size() == 0) {
					TRACEE("no result sql=%s ret=%d", (char*)sql, ret);
					return -4;
				}
				if (result.size() != 1) {
					TRACEE("more than one sql=%s ret=%d", (char*)sql, ret);
					return -5;
				}
				auto user1 = result.front();
				Buffer pwd = *user1->Fields["user_password"]->Value.String;
				TRACEI("password = %s", (char*)pwd);
				//登录请求的验证
				const char* MD5_KEY = "*&^%$#@b.v+h-b*g/h@n!h#n$d^ssx,.kl<kl";
				Buffer md5str = time + MD5_KEY + pwd + salt;
				Buffer md5 = Crypto::MD5(md5str);//MD5加密
				TRACEI("md5 = %s", (char*)md5);
				if (md5 == sign) {
					return 0;
				}
				return -6;
			}
		}
		else if (parser.Method() == HTTP_POST) {
			//post 处理
		}
		return -7;
	}
	Buffer MakeResponse(int ret) {//生成响应
		Json::Value root;
		root["status"] = ret;
		if (ret != 0) {
			root["message"] = "登录失败，可能是用户名或者密码错误！";
		}
		else {
			root["message"] = "success";
		}
		Buffer json = root.toStyledString();//json格式化
		Buffer result = "HTTP/1.1 200 OK\r\n";
		time_t t;
		time(&t);
		tm* ptm = localtime(&t);
		char temp[64] = "";
		strftime(temp, sizeof(temp), "%a, %d %b %G %T GMT\r\n", ptm);
		Buffer Date = Buffer("Date: ") + temp;
		Buffer Server = "Server: Edoyun/1.0\r\nContent-Type: text/html; charset=utf-8\r\nX-Frame-Options: DENY\r\n";
		snprintf(temp, sizeof(temp), "%d", json.size());
		Buffer Length = Buffer("Content-Length: ") + temp + "\r\n";
		Buffer Stub = "X-Content-Type-Options: nosniff\r\nReferrer-Policy: same-origin\r\n\r\n";
		result += Date + Server + Length + Stub + json;
		TRACEI("response: %s", (char*)result);
		return result;
	}
private:
	int ThreadFunc() {
		int ret = 0;
		EPEvents events;
		while (m_epoll != -1) {
			ssize_t size = m_epoll.WaitEvents(events);
			if (size < 0)break;
			if (size > 0) {
				for (ssize_t i = 0; i < size; i++)
				{
					if (events[i].events & EPOLLERR) {
						break;
					}
					else if (events[i].events & EPOLLIN) {
						CSocketBase* pClient = (CSocketBase*)events[i].data.ptr;
						if (pClient) {
							Buffer data;
							ret = pClient->Recv(data);
							TRACEI("recv data size %d", ret);
							if (ret <= 0) {
								TRACEW("ret= %d errno = %d msg = [%s]", ret, errno, strerror(errno));
								m_epoll.Del(*pClient);
								continue;
							}
							if (m_recvcallback) {
								(*m_recvcallback)(pClient, data);
							}
						}
					}
				}
			}
		}
		return 0;
	}
private:
	CEpoll m_epoll;
	std::map<int, CSocketBase*> m_mapClients;
	CThreadPool m_pool;
	unsigned m_count;
	CDatabaseClient* m_db;
};