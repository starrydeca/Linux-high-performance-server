#include<iostream>
using namespace std;
#include<sys/socket.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<poll.h>
#include<assert.h>
#include<stdlib.h>
#include<string.h>
#include<errno.h>
#include<fcntl.h>

#define BUF_SIZE 64
#define USER_LIMIT 5
#define FD_LIMIT 65525
struct client_data {
	sockaddr_in _addr;
	char* _write_buf;
	char _buf[BUF_SIZE];
};


void 
setnonblocking(int fd) {
	int flag = fcntl(fd, F_GETFL);
	flag |= O_NONBLOCK;
	fcntl(fd, F_SETFL, flag);
}

int 
main(int argc, char** argv) {
	if (argc < 4) {
		cout << "usage:" << basename(argv[0]) << "ip port backlog" << endl;
		return 1;
	}
	const char* ip = argv[1];
	int port = atoi(argv[2]);
	int backlog = atoi(argv[3]);

	int listenfd = socket(PF_INET, SOCK_STREAM, 0);
	assert(listenfd != -1);

	sockaddr_in sin;
	inet_pton(AF_INET, ip, &sin.sin_addr);
	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);
	int ret = bind(listenfd, (sockaddr*)&sin, sizeof(sin));
	assert(ret != -1);

	ret = listen(listenfd, backlog);
	
	client_data* users = new client_data[FD_LIMIT];
	int user_count = 0;
	
	pollfd pofds[USER_LIMIT + 1];
	pofds[0].fd = listenfd;
	pofds[0].events = POLLIN | POLLERR;
	pofds[0].revents = 0;
	for (int i = 1; i <= USER_LIMIT; i++) {
		pofds[i].fd = -1;
		pofds[i].events = 0;
	}

	while (1) {
		ret = poll(pofds, user_count + 1, -1);
		if (ret == -1) {
			cout << "poll failure" << endl;
			break;
		}
		for (int i = 0; i <= USER_LIMIT; i++) {
			if ((pofds[i].fd == listenfd) && (pofds[i].revents & POLLIN)) {
				sockaddr_in cin;
				socklen_t cin_len;
				int connfd = accept(listenfd, (sockaddr*)&cin, &cin_len);
				if (connfd < 0) {
					cout << "errno is:" << errno << endl;
					continue;
				}

				if (user_count >= USER_LIMIT) {
					const char* info = "too much user\n";
					cout << info;
					send(connfd, info, strlen(info), 0);
					close(connfd);
					continue;
				}

				user_count++;
				users[connfd]._addr = cin;
				setnonblocking(connfd);
				pofds[user_count].fd = connfd;
				pofds[user_count].events = POLLIN | POLLERR | POLLRDHUP;
				pofds[user_count].revents = 0;
				cout << "comes a new user, now have" << user_count << "users" << endl;
			}
			else if (pofds[i].revents & POLLERR) {
				cout << "get an error from" << pofds[i].fd << endl;
				char errors[100];
				memset(errors, 0, 100);
				socklen_t length = sizeof(errors);
				if (getsockopt(pofds[i].fd, SOL_SOCKET, SO_ERROR, &errors, &length) < 0){
					cout << "get socket option failed";
				}
				continue;
			}
			else if (pofds[i].revents & POLLRDHUP) {
				users[pofds[i].fd] = users[pofds[user_count].fd];
				close(pofds[i].fd);
				pofds[i].fd = pofds[user_count].fd;
				user_count--;
				i--;
				cout << "a client left" << endl;
			}
			else if (pofds[i].revents & POLLIN) {
				int connfd = pofds[i].fd;
				memset(users[connfd]._buf, 0, BUF_SIZE);
				ret = recv(connfd, users[connfd]._buf, BUF_SIZE - 1, 0);
				cout << "get" << ret << "bytes of client data" << users[connfd]._buf << "from" << connfd << endl;
				if (ret < 0) {
					if (errno != EAGAIN) {
						close(connfd);
						users[pofds[i].fd] = users[pofds[user_count].fd];
						pofds[i]  = pofds[user_count];
						i--;
						user_count--;
					}
				}
				else if (ret == 0) {

				}
				else {
					for (int j = 1; j <= user_count; j++) {
						if (pofds[j].fd == connfd)	continue;
						pofds[j].events |= ~POLLIN;
						pofds[j].events |= POLLOUT;
						users[pofds[j].fd]._write_buf = users[connfd]._buf;
					}
				}
			}
			else if (pofds[i].events & POLLOUT) {
				int connfd = pofds[i].fd;
				if (!users[connfd]._write_buf)	continue;
				ret = send(connfd, users[connfd]._write_buf, strlen(users[connfd]._write_buf), 0);
				users[connfd]._write_buf = NULL;
				pofds[i].events |= ~POLLOUT;
				pofds[i].events |= POLLIN;
			}
		}
	}

	delete [] users;
	close(listenfd);
	return 0;
}
