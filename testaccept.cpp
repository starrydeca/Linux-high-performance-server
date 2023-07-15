#include<iostream>//c++头文件
using namespace std;
#include<sys/socket.h>//socket、bind、listen、accept头文件
#include<arpa/inet.h>//inet_ntop、inet_pton头文件
#include<assert.h>//assert错误警告
#include<stdlib.h>//atoi字符串转换成整数
#include<string.h>//bzero清空sockaddr_in
#include<unistd.h>//sleep睡眠
#include<errno.h>//accept报错函数

int 
main (int argc, char* argv[]) {
	if (argc <= 3) {//检查参数个数是否正确
		cout << "usage: " << basename(argv[0]) << " ip" << " port_number" << " backlog" << endl;
	   return 0;	
	}
	char* ip = argv[1];//存储参数
	int port = atoi(argv[2]);
	int backlog = atoi(argv[3]);

	int sock = socket(PF_INET, SOCK_STREAM, 0);//创建套接子	
	assert(sock != -1);

	sockaddr_in address;//全双共TCP，创建自己详细的地址
	bzero(&address, sizeof(sockaddr_in));
	address.sin_family = AF_INET;
	inet_pton(AF_INET, ip, &address.sin_addr); //转换成网络字节序的整数
	address.sin_port = htons(port);//转换成网络字节序的short
	
	int ret = bind(sock, (const sockaddr*)&address, sizeof(sockaddr_in));//绑定
	assert(ret != -1);

	ret = listen(sock, backlog);//监听
	assert(ret != -1);

	sleep(20);
	
	sockaddr_in client;//客户端的socket地址
	socklen_t client_addrlen = sizeof(client);
	int connfd = accept(sock, (sockaddr*) &client, &client_addrlen);//连接，用客户端的socket地址接收
	if (connfd < 0)	cout << "erron is : " << errno << endl;//用if，还有else
	else {
		char remote[INET_ADDRSTRLEN];//用来存客户端ip
		cout << "connet ip is:" << inet_ntop(AF_INET, &client.sin_addr, remote, INET_ADDRSTRLEN) << " port is:" << ntohs(client.sin_port) << endl;
		close(connfd);//关闭客户端的文件描述符
	}
	close(sock);//关闭服务端的文件描述符
	return 0;
}
