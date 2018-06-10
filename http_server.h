#pragma once
#include <string>
#include <unordered_map>

namespace http_server
{
typedef std::unordered_map<std::string,std::string> Header;

//请求
struct Request
{
	std::string method;   //请求方法 GET/POST
	std::string url;
	// 例如 url 为一个形如 http://www.baidu.com/index.html?kwd="cpp"
	std::string url_path;	// /index.html
	std::string query_string;	//kwd="cpp"
	//std::string version; //暂时不考虑版本好号
	Header header;  //字符串键值对
	std::string body;  
};

//响应
struct Response
{
	int code;  //状态码
	std::string desc;	//状态码描述
	//std::string version; //暂时不考虑版本好号
	
	// 专门处理静态页面
	// 当前请求如果是静态页面
	Header header;
	std::string body;
	
	// 专门处理CGI来使用
	// cgi_resp 会被填充
	// 其余为空(header,body)
	std::string cgi_resp; // CGI程序返回给父进程的内容
						  // 包含了 header 和 body
						  // 引入这个变量，为了避免解析 CGI程序返回的内容
						  // 这部分内容可直接写入到socket中
};

//当前请求的上下文
//方便扩展
class HttpServer;
struct Context
{
	Request req;
	Response resp;
	int new_sock;
	HttpServer* server;
};

//实现核心流程的类
class HttpServer
{
public:
	//返回0成功，小于0失败
	int Start(const std::string& ip,short port);

private:
	// 根据HTTP请求字符串，进行反序列化
	// 从socket中读取一个字符串，输出Request对象
	int ReadOneRequest(Context* context);
	//根据Response对象，拼接成一个字副串，写回客户端
	int WriteOneResponse(Context* context);
	//根据Request对象，构造Responsed对象
	int HandlerRequest(Context* context);
	//静态解析
	int ProcessStaticFile(Context* context);
	int ProcessCGI(Context* context);
	//获取文件路径
	void GetFilePath(const std::string& url_path,std::string* file_path);
	//404
	int Process404(Context* context);

private:
	int ParseFirstLine(const std::string& first_line,
			std::string* method,std::string* url);
	static void* ThreadEntry(void* arg);
	int ParseUrl(const std::string& url,std::string* url_path,
			std::string* query_string);	
	int ParseHeader(const std::string& header_line,Header* header);
public: 
	//以下为测试函数
	void PrintRequest(const Request& req);
};

}	//end_http_server
