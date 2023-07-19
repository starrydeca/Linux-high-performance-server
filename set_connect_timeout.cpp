#include <iostream>
using namespace std;
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>

int 
timeout_connect(const char* ip, int port, int time) {
	int sockfd = socket(PF_INET, SOCK_STREAM, 0);
	assert(sockfd != -1);

	sockaddr_in sin;
	inet_pton(AF_INET, ip, &sin.sin_addr);
	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);
	
	timeval timeout;
	timeout.tv_sec = time;
	timeout.tv_usec = 0;
	int ret = setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
	assert(ret != -1);

	ret = connect(sockfd, (sockaddr*)&sin, sizeof(sin));
	if (ret < 0) {
		if (errno == EINPROGRESS) {
			cout << "connecting timeout, process timeout logic" << endl;
			return -1;
		}
		cout << "error ocur when connecting to server" << endl;
		return -1;
	}
	return sockfd;
}

int 
main(int argc, char** argv) {
	if (argc < 3) {
		cout << "usage:" << basename(argv[0]) << "ip port" << endl;
		return 1;
	}
	const char* ip = argv[1];
	int port = atoi(argv[2]);

	int sockfd = timeout_connect(ip, port, 10);
	if (sockfd < 0) return 1;
	return 0;
}
