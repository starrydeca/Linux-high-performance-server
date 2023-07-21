#pragma once

#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>
#define BUFFER_SIZE 64

class util_timer;

struct client_data {
	sockaddr_in cin;
	int fd;
	char buf[BUFFER_SIZE];
	util_timer* timer;//链表，所以直接存的是节点
};

class util_timer {
public:
	time_t expire;
	void (*cb_func)(client_data*);
	client_data* user_data;
	util_timer* pre;
	util_timer* next;
public:
	util_timer(): pre(NULL), next(NULL) {};
};

class sort_timer_lst {
private:
	util_timer* head;
	util_timer* tail;
public:
	sort_timer_lst(): head(NULL), tail(NULL) {};
	~sort_timer_lst();
	void add_timer(util_timer* timer);
	void adjust_timer(util_timer* timer);
	void del_timer(util_timer* timer);
	void trick();
private:
	void add_timer(util_timer* timer, util_timer* head);
};
