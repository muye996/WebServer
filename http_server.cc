#include "http_server.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sstream>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "util.hpp"
typedef struct sockaddr sockaddr;
typedef struct sockaddr_in sockaddr_in;

namespace http_server{


//启动服务器
int HttpServer::Start(const std::string& ip,short port)
{
	int listen_sock = socket(AF_INET,SOCK_STREAM,0);
	if(listen_sock < 0)
	{
		perror("socket");
		return -1;
	}
	// 解决TimeWait状态
	int opt = 1;
	setsockopt(listen_sock,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));

	sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(ip.c_str());
	addr.sin_port = htons(port);
	int ret = bind(listen_sock,(sockaddr*)&addr,sizeof(addr));
	if(ret < 0)
	{
		perror("bind");
		return -1;
	}
	ret = listen(listen_sock,5);
	if(ret < 0)
	{
		perror("listen");
		return -1;
	}
	//printf("ServerStart OK\n");
	LOG(INFO) << "SeerverStart OK!\n";
	while( 1 )
	{
		// 基于多线程
		sockaddr_in peer;
		socklen_t len = sizeof(peer);
		int new_sock = accept(listen_sock,(sockaddr*)&peer,&len);
		if(new_sock < 0)
		{
			perror("accept");
			continue;
		}
		//创建新线程，使用新线程完成请求
		Context* context = new Context();
		context->new_sock = new_sock;
		context->server = this;
		pthread_t tid;
		pthread_create(&tid,NULL,ThreadEntry,
						reinterpret_cast<void*>(context));
		pthread_detach(tid);
	}
	close(listen_sock);
	return 0;
}

//线程入口 静态
void* HttpServer::ThreadEntry(void* arg)
{
	//准备工作
	Context* context = reinterpret_cast<Context*>(arg);
	HttpServer* server = context->server;
	// 1.从文件描述符中读取对象，转换成 Request 对象
	int ret = 0;
	ret = server->ReadOneRequest(context);
	if(ret < 0)
	{
		LOG(ERROR) << "ReadOneRequest error!" << "\n";
		//失败 构造404 Response
		server->Process404(context);
		goto END;
	}
	// test 将解析出来的请求打印
	server->PrintRequest(context->req);
	// 2.把 Request 对象计算生成 Response 对象
	ret = server->HandlerRequest(context);
	if(ret < 0)
	{
		LOG(ERROR) << "HandlerRequest error!" << "\n";
		//失败 构造404 Response
		server->Process404(context);
		goto END;
	}
END:
	//TODO 处理失败
	// 3.把 Response 对象进行序列化，写回客户端
	server->WriteOneResponse(context);
	//收尾工作
	close(context->new_sock);
	delete context;
	return NULL;
}

//构造404响应对象
int HttpServer::Process404(Context* context)
{
	Response* resp = &context->resp;
	resp->code = 404;
	resp->desc = "Not Found";
	resp->body = "<head><meta http-equiv=\"content-type\" content=\"text/html\";\
				 charset=\"utf-8\"></head><h1>404 您的页面丢了^~^</h1>";
	std::stringstream ss;
	ss << resp->body.size();
	std::string size;
	ss >> size;
	resp->header["Content-Length"] = size;
	return 0;
}

//从 socket 中读取字符串，构造生成Request对象
//备注：1. 默认参数用于输入，const T&
//		2. 参数用于输出，T*
int HttpServer::ReadOneRequest(Context* context)
{
	Request* req = &context->req;
	//1.从socket中读取一行数据作为 Request 的首行
	//	按行读取的分隔符应该是 \n \r \r\n
	std::string first_line;
	FileUtil::ReadLine(context->new_sock,&first_line);
	//2.解析首行，获取请求的 method 和 url
	int ret = ParseFirstLine(first_line,&req->method,&req->url);
	if(ret < 0)
	{
		LOG(ERROR) << "ParseFirstLine error! first_line=" << first_line << "\n";
		return -1;
	}
	//3.解析url，获取 url_path 和 querry_string
	ret = ParseUrl(req->url,&req->url_path,&req->query_string);
	if(ret < 0)
	{
		LOG(ERROR) << "ParseUrl error! url="
					<< req->url << "\n";
		return -1;
	}
	//4.循环的按行读取数据，每次读取一行数据，就进
	//	行一次 header 的解析，读到空行，header结束
	std::string header_line;
	while(1)
	{
		FileUtil::ReadLine(context->new_sock,&header_line);
		//如果 header_line 是空行 退出
		//由于readline 读到空行时 返回的header_line 是空
		if(header_line == "")
		{
			break;
		}
		ret = ParseHeader(header_line,&req->header);
		if(ret < 0)
		{
			LOG(ERROR) << "ParseUrl header! header_line="
						<< header_line << "\n";
			return -1;
		}
	}
	//5.如果是 POST 请求，但是没有 Content-Length，请求失败
	Header::iterator it = req->header.find("Content-Length");
	if(req->method == "POST"
		&& it == req->header.end())
	{
		LOG(ERROR) << "POST Request has no Content-length!\n";
		return -1;
	}
	// 	如果是GET 请求，不用读取 body
	if(req->method == "GET")
	{
		return 0;
	}
	//  如果是 POST 请求，并且 header 中包含了Content-Length
	// 	字段 继续读取socket，获取 body 中的内容
	int content_length = atoi(it->second.c_str());
	ret = FileUtil::ReadN(context->new_sock,content_length,&req->body);
	if(ret < 0)
	{
		LOG(ERROR) << "ReadN error! content_length="
					<< content_length << "\n";
		return -1;
	}
	return 0;
}
//解析首行，按照空格划分 
int HttpServer::ParseFirstLine(const std::string& first_line,
			std::string* method,std::string* url)
{
	std::vector<std::string> tokens;
	StringUtil::Splict(first_line," ",&tokens);
	if (tokens.size() != 3)
	{
		// 首行格式不对
		LOG(ERROR) << "ParseFirstLine error! splict error! first_line="
				<< first_line << "\n";
		return -1;
	}
	//如果版本号不包含HTTP,也认为出错
	if (tokens[2].find("HTTP") == std::string::npos)
	{
		LOG(ERROR) << "ParseFirstLne error! version error! first_line="
				<< first_line << "\n";
		return -1;
	}
	*method = tokens[0];
	*url = tokens[1];
	return 0;
}

//解析Url
//解析一个标准的url，其实比较复杂,核心思路是以 ? 分割， ?左边path 右边uerry_string
//此处实现简化版本，之考虑不包含域名和协议以及#的情况
int HttpServer::ParseUrl(const std::string& url,std::string* url_path,
			std::string* query_string)
{
	size_t pos = url.find("?");
	//没找到
	if (pos == std::string::npos)
	{
		*url_path = url;
		*query_string = "";
		return 0;
	}
	*url_path = url.substr(0,pos);
	*query_string = url.substr(pos+1);
	return 0;
}

// 解析一行header
// 此处使用 string::find 来进行实现
int HttpServer::ParseHeader(const std::string& header_line,Header* header)
{
	size_t pos = header_line.find(":");
	//无 :
	if(pos == std::string::npos)
	{
		LOG(ERROR) << "ParseHeader error!has no : header_line="
				<< header_line << "\n";
		return -1;
	}
	// 没有 value
	if (pos + 2 >= header_line.size())
	{
		LOG(ERROR) << "ParseHeader error! has no value! header_line="
				<< header_line << "\n";
		return -1;
	}
	(*header)[header_line.substr(0,pos)]
		= header_line.substr(pos+2);
	return 0;
}

// 实现序列化，把 Response 对象转换成一个 string
// 写回到 socket 中
int HttpServer::WriteOneResponse(Context* context)
{
	// iostream 和 stringstream 类似于 printf 和 sprintf 之间的关系，
	// sprintf 可自动分配空间
	// 1.进行序列化
	Response& resp = context->resp;
	std::stringstream ss;
	ss << "HTTP/1.1 " << resp.code << " " << resp.desc << "\n";
	// 静态
	if (resp.cgi_resp == "")
	{
		for(auto item : resp.header)
		{
			ss << item.first << ": " << item.second << "\n";
		}
		ss << "\n";
		ss << resp.body;
	}
	// cgi cgi_resp 同时包含了响应数据的 header 空行 和 body
	else 
	{
		ss << resp.cgi_resp;
	}
	// 2.将序列化的结果写到 socket 中
	const std::string& str = ss.str();
	write(context->new_sock,str.c_str(),str.size());
	return 0;
}

// 通过输入的 Request 对象计算生成 Response 对象
// 1.静态文件
//  a)GET 并没有query_string 作为参数
// 2.动态生成页面
//  a)GET 并且 query_string 作为参数
//  b)POST 请求
int HttpServer::HandlerRequest(Context* context)
{
	const Request& req = context->req;
	Response* resp = &context->resp;
	resp->code = 200;
	resp->desc = "OK";
	// 判定当前处理方式
	if(req.method == "GET" && req.query_string == "")
	{
		return context->server->ProcessStaticFile(context);
	}
	else if((req.method == "GET" && req.query_string != "")
			|| req.method == "POST")
	{
		return context->server->ProcessCGI(context);
	}
	else // 其他方法 暂时没有支持
	{
		LOG(ERROR) << "Unsupport Method! method" << req.method << "\n";
	}
	//return -1; //测试响应
	return 0;
}

// 1.通过 Request 中的 url_path 字段，计算出磁盘路径
// 	 例如 url_path /index.html,想要得到磁盘上的路径为 ./wwwroot/index.html
// 	 例如 url_path /image/1.jpg
// 	 想得到磁盘上的文件爱你就是 ./wwwroot/image/1.jpg
// 	 备注： ./wwwroot 是拟定的根目录
// 2.打开文件，将文件中的内容读取出来放到 body 中
//
int HttpServer::ProcessStaticFile(Context* context)
{
	const Request& req = context->req;
	Response* resp = &context->resp;
	// 1.获取静态文件的完整路径
	std::string file_path;
	GetFilePath(req.url_path,&file_path);
	// 2.打开并读取完整的文件
	int ret = FileUtil::ReadAll(file_path,&resp->body);
	if (ret<0)
	{
		LOG(ERROR) << "ReadAll error! file_path=" << file_path << "\n";
		return -1;
	}
	return 0;
}

// 通过 url_path 找到对应的文件路径 
// 例如 url_path 可能是 http://192.168.80.132:9090/
// url_path 是 /
// 例如 url_path 可能是 http://192.168.80.132:9090/image/
// url_path 是 /image/
// 如果 url_path 指向的是一个目录，就尝试在这个目录下访问一个叫做
// index.html 的文件
void HttpServer::GetFilePath(const std::string& url_path,
							std::string* file_path)
{
	*file_path = "./wwwroot" + url_path;
	// 判定一个路径是普通文件还是目录文件
	// 1. linux的 stat 函数
	// 2.通过 boost filesystem 模块
	if(FileUtil::IsDir(*file_path))
	{
		// 1. /image/
		// 2. /imahe
		if(file_path->back() != '/')
		{
			file_path->push_back('/');
		}
		(*file_path) +=  "index.html";
	}
	return;
}


int HttpServer::ProcessCGI(Context* context)
{
	const Request& req = context->req;
	Response* resp = &context->resp;
	// 1.创建匿名管道（父子进程双向通信）
	int fd1[2],fd2[2];
	pipe(fd1);
	pipe(fd2);
	int father_write = fd1[1];
	int child_read   = fd1[0]; 
	int father_read  = fd2[0];
	int child_write  = fd2[1];
	// 2.设置环境变量
	// 	a) METHOD 请求方法
 	std::string env = "REQUEST_METHOD=" + req.method;
	putenv(const_cast<char*>(env.c_str()));
	if (req.method == "GET")
	{
		// 	b) GET 方法 QUERY_STRING 请求的参数
		env = "QUERY_STRING=" + req.query_string;
		putenv(const_cast<char*>(env.c_str()));
	}
	else if (req.method == "POST")
	{
		// 	c) POST 方法，就设置CONTENT_LENGTH
		auto pos = req.header.find("Content-Length");
		env = "CONTENT_LENGTH=" + pos->second;
		putenv(const_cast<char*>(env.c_str()));
	}

	pid_t ret = fork();
	if (ret < 0 )
	{
		perror("fork");
		goto END;
	}
	// 3.fork 父进程流程
	if (ret > 0 )
	{
		close(child_read);
		close(child_write);
		//  a) 如果是POST请求，父进程就要把 body  写入管道中
		if (req.method == "POST")
		{
			write(father_write,req.body.c_str(),req.body.size());
		}
		//  b) 阻塞式读取管道，尝试把子进程的结果读取出来
		//     并且放到 Response 对象中
		FileUtil ::ReadAll(father_read,&resp->cgi_resp);
		//  c) 对字进程进行进程等待(为了避免僵尸进程)
		wait(NULL);
	}
	else
	{
		// 4. fork，子进程流程
		close(father_read);
		close(father_write);
		//  a) 把标准输入和标准输出进行重定向
		dup2(child_read,0);
		dup2(child_write,1);
		//  b）先获取到要替换的可执行文件是哪个(通过 url_path 来获取)
		std::string file_path;
		GetFilePath(req.url_path,&file_path);
		//  c）进行进程的程序替换
		execl(file_path.c_str(),file_path.c_str(),NULL);
		//  d）由我们的CGI可执行程序完成动态页面的计算，并且写回数据到管道中
		//   这部分逻辑，需要放到单独文件实现，并根据该文件生成可执行程序
	}
END:
	// 统一处理收尾工作
	close(father_read);
	close(child_read);
	close(father_write);
	close(child_write);
	return 0;
}	

///////////////////////////////////////
// 以下为测试函数
///////////////////////////////////////
void HttpServer::PrintRequest(const Request& req)
{
	LOG(DEBUG) << "Request::" << "\n"
		<< req.method << " " << req.url << "\n"
		<< req.url_path << " " << req.query_string << "\n";
	for(auto it : req.header)
	{
		LOG(DEBUG) << it.first << " " << it.second << "\n";
	}
	LOG(DEBUG) << "\n";
	LOG(DEBUG) << req.body << "\n";
}
//////////////////////////////////////

} //end http_server

