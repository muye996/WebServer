//此文件放置工具类

#pragma once
#include <iostream>
#include <sys/time.h>
#include <sys/socket.h>
#include <vector>
#include <fstream>
#include <unordered_map>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>

class TimeUtil
{
public:
	//获取当前的秒级时间戳
	static int64_t TimeStamp()
	{
		struct timeval tv;
		gettimeofday(&tv,NULL);
		return tv.tv_sec;
	}
	//获取当前的微秒级时间戳
	static int64_t TimeUStamp()
	{
		struct timeval tv;
		gettimeofday(&tv,NULL);
		return 1000 * 1000 * tv.tv_sec + tv.tv_usec;
	}
};

enum LogLevel
{
	DEBUG,
	INFO,
	WARNING,
	ERROR,
	CRITIAL,
};

static std::ostream& Log(LogLevel level,const char* file,int line)
{
	std::string prefix = "I";
	if(level == WARNING)	prefix = "W";
	else if(level == ERROR) prefix = "E";
	else if(level == DEBUG) prefix = "D";
	else if(level == CRITIAL) prefix = "C";
	std::cout << "[" << prefix << TimeUtil::TimeStamp() << " "
			<< file << ":" << line << "]";

	return std::cout;
}

// __FILE__ __LINE__
#define LOG(level) Log(level,__FILE__,__LINE__)

class FileUtil 
{
public:
	//从文件描述符中读取一行 返回的line不包含界定标识
	static int ReadLine(int fd,std::string* line)
	{
		/*string tmp;
		line->swap(tmp);*/
		line->clear();
		while (true)
		{
			char c = '\0';
			ssize_t read_size = recv(fd,&c,1,0);
			if (read_size <= 0) {
				return -1;
			}
			if (c == '\r') {
				// 虽然从缓冲区读取了一个字符，但是并没有删除
				recv(fd,&c,1,MSG_PEEK);
				if(c == '\n'){
					// 发现 \r 后面刚好是一个 \n
					recv(fd,&c,1,0);
				}else {
					c = '\n';
				}
			}
			if (c == '\n') {
				break;
			}
			line->push_back(c);
		}
		return 0;
	}

	static int ReadN(int fd,size_t len,std::string* output)
	{
		output->clear();
		char c = '\0';
		for(size_t i=0; i<len; i++)
		{
			recv(fd,&c,1,0);
			output->push_back(c);
		}
		return 0;
	}

	// 从文件中读取全部内容到 std::string 中
	static int ReadAll(const std::string& file_path,std::string* output)
	{
		std::ifstream file(file_path.c_str());
		if(!file.is_open())
		{
			LOG(ERROR) << "Open file error! file_path=" << file_path << "\n";
			return -1;
		}
		// seekg 调整文件指针的位置
		file.seekg(0,file.end);
		// 查询当前文件指针的位置，返回值相当于文件起始位置偏移量
		int length = file.tellg();
		file.seekg(0,file.beg);
		//读取完整的文件内容
		output->resize(length);
		file.read(const_cast<char*>(output->c_str()),length);

		// 忘记写close问题不大，有C++析构
		file.close();
		return 0;
	}

	static int ReadAll(int fd,std::string* output)
	{
		while(true)
		{
			char buf[1024] = {0};
			ssize_t read_size = read(fd,buf,sizeof(buf)-1);
			if (read_size < 0)
			{
				LOG(INFO) << read_size << "\n";
				perror("read");
				return -1;
			}
			if (read_size == 0)
			{
				// 文件读取结束
				return 0;
			}
			buf[read_size] = '\0';
			(*output) += buf;
		}
		return 0;
	}

	static bool IsDir(const std::string& file_path)
	{
		return boost::filesystem::is_directory(file_path);
	}


};

class StringUtil
{
public:
	// 把一个字符串按照 splict_char 切分 放到 output 数组中
	static int Splict(const std::string& input,const std::string& splict_char,
			std::vector<std::string>* output)
	{
		boost::split(*output,input,
					boost::is_any_of(splict_char), //多个
					boost::token_compress_on);	//切分结果压缩打开  例如分隔符是空格，
												//"a b"  返回 "a" "b"
												//token_compress_off "a b" 返回"a",""."b"
		return 0;
	}
	
	typedef std::unordered_map<std::string,std::string> UrlParam;
	static int ParseUrlParam(const std::string& input,UrlParam* output)
	{
		// 1.先按照取地质符号切分成若干个 kv
		std::vector<std::string> params;
		Splict(input,"&",&params);
		// 2.在针对每一个kv，按照 = 切分
		for (auto item : params)
		{
			std::vector<std::string> kv;
			Splict(item,"=",&kv);
			if (kv.size() != 2)
			{
				LOG(WARNING) << "kv format error! kv item=" << item << "\n";
				continue;
			}
			(*output)[kv[0]] = kv[1];
		}
		return 0;
	}

};
