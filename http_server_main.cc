#include "http_server.h"
#include <iostream>
using namespace http_server;

int main(int argc,char* argv[])
{
	if(argc != 3)
	{
		std::cout << "Usage" << std::endl;
		return 1;
	}
	HttpServer server;
	server.Start(argv[1],atoi(argv[2]));
}
