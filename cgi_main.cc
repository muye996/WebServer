////////////////////
// 字符串转数字
// 1.atoi
// 2.sscanf
// 3.stringstream
// 4.boost lexical_cast
// s.stoi(C++11)
//
// 数字转字符串
// 1.itoa
// 2.sprintf
// 3.stringstram
// 4.boost lexical_cast
// 5.to_string (C++11)
//
#include <iostream>
#include <string>
#include <sstream>
#include "util.hpp"

void HttpResponse(const std::string& body)
{
	std::cout << "Content-Length " << body.size() << "\n";
	std::cout << "\n";
	std::cout << body;
	return;
}
// 这个代码要生成我们的 CGI 程序，通过 CGI 完成具体的业务
// 本CGI 程序完成两个数的加法
int main()
{
	// 1.先获得 method
	const char* method = getenv("REQUEST_METHOD");
	if (method == NULL)
	{
		HttpResponse("No env Method");
		return 1;
	}
	// 2.如果是 GET 请求，从 QUERY_STRING 中读取数据
	// 4.解析 query_string 或者 body 中的数据
	StringUtil::UrlParam params;
	if (std::string(method) == "GET")
	{
		const char* query_string = getenv("QUERY_STRING");
		StringUtil::ParseUrlParam(query_string,&params);
	}
	else if (std::string(method) == "POST")
	{
		// 3.如果是 POST 请求，从 CONTENT_LENGTH 读取 body 的长度
		//   根据 body 的长度，从标准输入中读取请求的 body
		// 4.解析 query_string 或者 body 中的数据
		const char* content_length = getenv("CONTENT_LENGTH");
		char buf[1024*10] = {0};
		read(0,buf,sizeof(buf)-1);
		StringUtil::ParseUrlParam(buf,&params);
	}
	// 5.根据业务需要进行计算，此处计算 a+b 的值
	int a = std::stoi(params["a"]);
	int b = std::stoi(params["b"]);
	int result = a + b;
	// 6.根据计算的结果，构造响应的数据，写回到标准输出中
	std::stringstream ss;
	ss << "<h1> result = " << result << "</h1>";
	HttpResponse(ss.str());
	return 0;
}
