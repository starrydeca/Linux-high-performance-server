#include<iostream>
using namespace std;
#include<sys/socket.h>//socket、bind、listen、accept、recv、send 
#include<arpa/inet.h>//inet_pton、htons 
#include<unistd.h>//close、pipe
#include<poll.h>//poll、pollfd 
#include<assert.h>//assert 
#include<stdlib.h>//atoi 
#include<string.h>//basename 
#include<errno.h>//errno 
#include<fcntl.h>//fcntl、splice

#define BUF_SIZE 64
#define USER_LIMIT 5
#define FD_LIMIT 65525
struct client_data {//客户结构体
	sockaddr_in _addr;
	char* _write_buf;
	char _buf[BUF_SIZE];
};


void 
setnonblocking(int fd) {//设置非阻塞
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
	
	client_data* users = new client_data[FD_LIMIT];//开辟空间，已链接客户
	int user_count = 0;//已链接客户人数
	
	pollfd pofds[USER_LIMIT + 1];//创建接待客户的poll
	pofds[0].fd = listenfd;//第一个到达的listenfd放入poll
	pofds[0].events = POLLIN | POLLERR;
	pofds[0].revents = 0;
	for (int i = 1; i <= USER_LIMIT; i++) {//其他的poll初始化
		pofds[i].fd = -1;
		pofds[i].events = 0;
	}

	while (1) {
		ret = poll(pofds, user_count + 1, -1);
		if (ret == -1) {
			cout << "poll failure" << endl;
			break;
		}
		for (int i = 0; i <= USER_LIMIT; i++) {//论寻每个pollfd，有没有事件触发
			if ((pofds[i].fd == listenfd) && (pofds[i].revents & POLLIN)) {//新链接到达
				sockaddr_in cin;
				socklen_t cin_len;
				int connfd = accept(listenfd, (sockaddr*)&cin, &cin_len);//连接
				if (connfd < 0) {//有没有连接错误
					cout << "errno is:" << errno << endl;
					continue;
				}

				if (user_count >= USER_LIMIT) {//超过既定的人数
					const char* info = "too much user\n";
					cout << info;
					send(connfd, info, strlen(info), 0);
					close(connfd);
					continue;
				}
				//没有超过既定人数
				user_count++;
				users[connfd]._addr = cin;
				setnonblocking(connfd);
				pofds[user_count].fd = connfd;
				pofds[user_count].events = POLLIN | POLLERR | POLLRDHUP;
				pofds[user_count].revents = 0;
				cout << "comes a new user, now have" << user_count << "users" << endl;
			}
			else if (pofds[i].revents & POLLERR) {//出发错误事件
				cout << "get an error from" << pofds[i].fd << endl;
				char errors[100];
				memset(errors, 0, 100);
				socklen_t length = sizeof(errors);
				if (getsockopt(pofds[i].fd, SOL_SOCKET, SO_ERROR, &errors, &length) < 0){//把句柄上的错误信息剪切到errors上
					cout << "get socket option failed" << errors << endl;
				}
				continue;
			}
			else if (pofds[i].revents & POLLRDHUP) {//客户端断开连接
				users[pofds[i].fd] = users[pofds[user_count].fd];//把最后一个客户放到现在的位置上
				close(pofds[i].fd);//断开现在的位置上的句柄
				pofds[i].fd = pofds[user_count].fd;//把最后一个fd放到现在的位置上
				user_count--;//人数减一
				i--;//轮寻减一，重新检测，因为这个位置上换了句柄
				cout << "a client left" << endl;
			}
			else if (pofds[i].revents & POLLIN) {//可读
				int connfd = pofds[i].fd;
				memset(users[connfd]._buf, 0, BUF_SIZE);//清空缓冲区
				ret = recv(connfd, users[connfd]._buf, BUF_SIZE - 1, 0);//接收数据
				cout << "get" << ret << "bytes of client data" << users[connfd]._buf << "from" << connfd << endl;
				if (ret < 0) {
					if (errno != EAGAIN) {//发生错误
						close(connfd);
						users[pofds[i].fd] = users[pofds[user_count].fd];
						pofds[i]  = pofds[user_count];
						i--;
						user_count--;
					}
				}
				else if (ret == 0) {

				}
				else {//没有发生错误
					for (int j = 1; j <= user_count; j++) {//把其他句柄，都变成可写状态，客户结构也加上刚刚接收到的数据
						if (pofds[j].fd == connfd)	continue;
						pofds[j].events |= ~POLLIN;
						pofds[j].events |= POLLOUT;
						users[pofds[j].fd]._write_buf = users[connfd]._buf;
					}
				}
			}
			else if (pofds[i].events & POLLOUT) {//可写
				//把句柄，从可写状态改成可读状态
				pofds[i].events |= ~POLLOUT;
				pofds[i].events |= POLLIN;
			
				int connfd = pofds[i].fd;
				if (!users[connfd]._write_buf)	continue;//缓冲区为空，不可写直接返回
				ret = send(connfd, users[connfd]._write_buf, strlen(users[connfd]._write_buf), 0);//发送数据
				users[connfd]._write_buf = NULL;//清空写缓冲区
			
			}
		}
	}

	delete [] users;
	close(listenfd);
	return 0;
}
