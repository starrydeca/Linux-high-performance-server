#include <iostream>
using namespace std;
#include <sys/socket.h>//socket、bind、accept
#include <arpa/inet.h>//inet_pton、htons 
#include <poll.h> //poll、pollfd、
#include <unistd.h>//close、pipe 
#include <assert.h>//assert 
#include <string.h>//basename 
#include <stdlib.h>//atoi 
#include <fcntl.h> //splice 
#define BUF_SIZE 64

int 
main (int argc, char** argv) {
	if (argc < 3) {
		cout << "usag:" << basename(argv[0]) << "ip port" << endl;
		return 0;
	}
	const char* ip = argv[1];
	int port = atoi(argv[2]);

	int sockfd = socket(PF_INET, SOCK_STREAM, 0);
	assert(sockfd != -1);

	sockaddr_in sin;
	inet_pton(AF_INET, ip, &sin.sin_addr);
	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);

	int ret = connect(sockfd, (sockaddr*)&sin, sizeof(sin));
	if (ret < 0) {
		cout << "connect failed" << endl;
		return 1;
	}

	pollfd pofds[2];
	pofds[0].fd = 0;//创建文件描述符，监听pipe
	pofds[0].events = POLLIN;
	pofds[0].revents = 0;
	pofds[1].fd = sockfd;//监听sockfd
	pofds[1].events = POLLIN | POLLRDHUP;//可读、断开链接
	pofds[1].revents = 0;

	char read_buf[BUF_SIZE];
	int pipefd[2];//读 + 写
    ret = pipe(pipefd);
	assert(ret != -1);

	while (1) {
		ret = poll(pofds, 2, -1);
		if (ret == -1) {
			cout << "poll failure" << endl;
			break;
		}

		if (pofds[1].revents & POLLRDHUP) {//服务器断开链接
			cout << "server close the connection" << endl;
			break;
		}
		else if (pofds[1].revents & POLLIN){//服务器传数据来了，可读
			memset(read_buf, 0, BUF_SIZE);//清空缓冲区
			recv(pofds[1].fd, read_buf, BUF_SIZE - 1, 0);//开始读
			cout << read_buf << endl;//打印读的内容
		}
		else if (pofds[0].revents & POLLIN) {//用键盘，在终端写数据
			splice(0, NULL, pipefd[1], NULL, 32768, SPLICE_F_MORE | SPLICE_F_MOVE);//从终端的数据零拷贝到管道pipefd[1]中
			splice(pipefd[0], NULL, sockfd, NULL, 32768, SPLICE_F_MORE | SPLICE_F_MOVE);//从管道pipe[0]中的数据零拷贝到sockfd中
		}
	}
	close(sockfd);
	return 0;
}
