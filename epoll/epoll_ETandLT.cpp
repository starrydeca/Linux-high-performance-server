#include<iostream>
using namespace std;
#include<sys/socket.h>//socket、bind、listen、accept、recv、send 
#include<arpa/inet.h>//inet_pton、inet_ntop、htons、ntohs
#include<stdlib.h>//atoi 
#include<string.h>//basename 
#include<sys/epoll.h>//epoll_event、epoll_create、epoll_wait、epoll_ctl 
#include<assert.h>//assert 
#include<fcntl.h>//fcntl 
#include<unistd.h>//close 

#define MAX_EVENT_NUMBER 1024
#define BUF_SIZE 10

int 
setnonblocking(int fd) {//设置非阻塞
	int flag = fcntl(fd, F_GETFL);
	flag |= O_NONBLOCK;
	flag = fcntl(fd, F_SETFL, flag);
	return flag;
}

void 
addfd(int epfd, int fd, bool flag) {//把句柄添加到epoll
	epoll_event event;
	event.data.fd = fd;
	event.events = EPOLLIN;
	if (flag) event.events |= EPOLLET;//判断是否设置为边缘触发
	epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &event);
	setnonblocking(fd);//设置非阻塞
}

void 
lt(int epfd, epoll_event* events, int num, int fd) {//设置水平触发
	char buf[BUF_SIZE];
	for (int i = 0; i < num; i++) {
		int sockfd = events[i].data.fd;
		if (sockfd == fd) {//新连接就加入epoll
			sockaddr_in cin;
			socklen_t cin_len;
			int connfd = accept(sockfd, (sockaddr*)&cin, &cin_len);
			assert(connfd != -1);
			addfd(epfd, connfd, 0);
		}
		else if (events[i].events & EPOLLIN) {//是接收数据，就接收
			cout << "event trigger once" << endl;
			memset(buf, 0, BUF_SIZE);
			int ret = recv(sockfd, buf, BUF_SIZE - 1, 0);
			if (ret <= 0) {
				close(sockfd);
				continue;
			}
			cout << "get" << ret << "bytes of content:" << buf << endl;
		}
		else cout << "else someting happened" << endl;
	}
}

void et(int epfd, epoll_event* events, int num, int fd) {//设置边缘触发
	char buf[BUF_SIZE];
	for (int i = 0; i < num; i++) {
		int sockfd = events[i].data.fd;
		if (sockfd == fd) {
			sockaddr_in cin;
			socklen_t cin_len;

			int connfd = accept(sockfd, (sockaddr*)&cin, &cin_len);
			assert(connfd != -1);

			addfd(epfd, connfd, 1);
		}
		else if (events[i].events & EPOLLIN) {
			cout << "epoll trigger once" << endl;
			while (1) {//确保读完
				memset(buf, 0, BUF_SIZE);
				int ret = recv(sockfd, buf, BUF_SIZE - 1, 0);
				if (ret < 0) {
					if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {//返回异常
						cout << "read late" << endl;
						break;
					}
					close(sockfd);//没有异常
					break;
				}
				else if (ret == 0)	close(sockfd);//正好读完
				else cout << "get" << ret << "bytes of content:" << buf << endl;//正在读
			}
		}
		else cout << "something else happened" << endl;
	}
}

int 
main(int argc, char** argv) {
	if (argc < 4) {
		cout << "usage:" << basename(argv[0]) << "ip port backlog" << endl;
		return 0;
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
	assert(ret != -1);

	epoll_event events[MAX_EVENT_NUMBER];//创建事件集——epoll是事件驱动
	int epfd = epoll_create(1);//创建根节点
	assert(epfd != -1);

	addfd(epfd, listenfd, true);//加入红黑树，et or lt 无所谓
	
	while (1) {
		ret = epoll_wait(epfd, events, MAX_EVENT_NUMBER, -1);//用epoll_wait判断这颗红黑树，有那些节点触发了事件，并且把他们放到前面
		if (ret == -1) {
			cout << "epoll failure" << endl;
			break;
		}
//		lt(epfd, events, ret, listenfd);//水平触发
		et(epfd, events, ret, listenfd);//边缘触发
	}
	close(listenfd);
	return 0;
}
