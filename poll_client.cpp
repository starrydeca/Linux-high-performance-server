#include <iostream>
using namespace std;
#include <sys/socket.h>
#include <arpa/inet.h>
#include <poll.h> 
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h> 
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
	pofds[0].fd = 0;
	pofds[0].events = POLLIN;
	pofds[0].revents = 0;
	pofds[1].fd = sockfd;
	pofds[1].events = POLLIN | POLLRDHUP;
	pofds[1].revents = 0;

	char read_buf[BUF_SIZE];
	int pipefd[2];
    ret = pipe(pipefd);
	assert(ret != -1);

	while (1) {
		ret = poll(pofds, 2, -1);
		if (ret == -1) {
			cout << "poll failure" << endl;
			break;
		}

		if (pofds[1].revents & POLLRDHUP) {
			cout << "server close the connection" << endl;
			break;
		}
		else if (pofds[1].revents & POLLIN){
			memset(read_buf, 0, BUF_SIZE);
			recv(pofds[1].fd, read_buf, BUF_SIZE - 1, 0);
			cout << read_buf << endl;
		}
		else if (pofds[0].revents & POLLIN) {
			splice(0, NULL, pipefd[1], NULL, 32768, SPLICE_F_MORE | SPLICE_F_MOVE);
			splice(pipefd[0], NULL, sockfd, NULL, 32768, SPLICE_F_MORE | SPLICE_F_MOVE);
		}
	}
	close(sockfd);
	return 0;
}
