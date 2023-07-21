#include "lst_timer.h"
#include <iostream>
using namespace std;

sort_timer_lst::~sort_timer_lst() {
	util_timer* tmp = head;
	while (tmp) {
		head = head->next;
		delete(tmp);
		tmp = head;
	}
}

void 
sort_timer_lst::add_timer(util_timer* timer) {
	if (!timer)	return ;
	if (!head) {
		head = tail = timer;
		return ;
	}
	if (timer->expire < head->expire) {
		timer->next = head;
		head->pre = timer;
		head = timer;
		return ;
	}
	add_timer(timer, head);
}

void 
sort_timer_lst::add_timer(util_timer* timer, util_timer* head) {
	util_timer* tmp = head;
	while(tmp) {
		if (tmp->expire > timer->expire) {
			tmp = tmp->pre;
			tmp->pre->next = timer;
			timer->next = tmp->next;
			return;
		}
		tmp = tmp->next;
	}
	tail->next = timer;
	tail = timer;
}

void
sort_timer_lst::adjust_timer(util_timer* timer) {
	if (!timer)	return ;

	util_timer* tmp = timer->next;
	if (!tmp || timer->expire <= tmp->expire)	return;

	if (timer == head) {
		head = head->next;
		head->pre = NULL;
		timer->next = NULL;
		add_timer(timer, head);
	}
	else {
		timer->pre->next = timer->next;
		timer->next->pre = timer->pre;
		add_timer(timer, head);
	}
}

void
sort_timer_lst::del_timer(util_timer* timer) {
	if (!timer)	return;
	if (timer == head && timer == tail) {
		delete timer;
		head = NULL;
		tail = NULL;
		return ;
	}

	if (timer == head) {
		head = head->next;
		head->pre = NULL;
		delete timer;
		return ;
	}
	else if (timer == tail) {
		tail = tail->pre;
		tail->next = NULL;
		delete timer;
		return ;
	}

	timer->pre->next = timer->next;
	timer->next->pre = timer->pre;
	delete timer;
}

void
sort_timer_lst::trick() {
	if (!head)	return;

	cout << "time trick" << endl;

	time_t cur = time(NULL);
	util_timer* tmp = head;
	while (1) {
		if (cur < head->expire)	break;

		tmp->cb_func(tmp->user_data);
		head = head->next;
		if (head)	head->pre = NULL;
		delete tmp;
		tmp = head;
	}
}
