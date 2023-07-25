#include <iostream>
using namespace std;
#include "lst_timer.h"
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/epoll.h>
#include <signal.h> 
#define FD_LIMIT 65535
#define MAX_EVENT_NUMER 1024
#define TIMESLOT 5

static int pipefd[2];
static sort_timer_lst timer_lst;
static int epfd = 0;

void 
setnonblocking(int fd) {
    int flag = fcntl(fd, F_GETFL);
    flag |= O_NONBLOCK;
    fcntl(fd, F_SETFL, flag);
}

void
addfd(int epfd, int fd) {
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;
    epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

void 
sig_handler(int sig) {
    int save_errno = errno;
    int msg = sig;
    send(pipefd[1], (char*)&msg, 1, 0);
    errno = save_errno;    
}

void 
addsig(int sig) {
    struct    sigaction sa;
    sa.sa_handler = sig_handler;
    sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

void cb_func(client_data* user_data) {
    epoll_ctl(epfd, EPOLL_CTL_DEL, user_data->fd, 0);
    assert(user_data);
    close(user_data->fd);
    cout << "close fd " << user_data->fd << endl;
}

void 
timer_handler() {
    timer_lst.trick();
    alarm(TIMESLOT);
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

    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(listenfd != -1);

    sockaddr_in sin;
    inet_pton(AF_INET, ip, &sin.sin_addr);
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);

    int ret = bind(listenfd, (sockaddr*)&sin, sizeof(sin));
    assert(ret != -1);

    ret = listen(listenfd, backlog);
    assert(ret != -1);

    epoll_event events[MAX_EVENT_NUMER];
    int epfd = epoll_create(5);
    assert(epfd != -1);
    addfd(epfd, listenfd);
    
    addsig(SIGALRM);
    addsig(SIGTERM);
    bool stop_server = 0;

    client_data* users = new client_data[FD_LIMIT];
    bool timeout = 0;
    alarm(TIMESLOT);

    while (!stop_server) {
        int number = epoll_wait(epfd, events, MAX_EVENT_NUMER, -1);
        if (number < 0 && (errno != EINTR)) {
            cout << "epoll failure" << endl;
            break;
        }

        for (int i = 0; i < number; i++) {
            int sockfd = events[i].data.fd;
            if (sockfd == listenfd) {
                sockaddr_in cin;
                socklen_t cin_len;
                int connfd = accept(listenfd, (sockaddr*)&cin, &cin_len);

                addfd(epfd, connfd);
                users[connfd].cin = cin;
                users[connfd].fd = connfd;
    
                util_timer* timer = new util_timer;
                timer->user_data = &users[connfd];
                timer->cb_func = cb_func;
                time_t cur = time(NULL);
                timer->expire = cur + 3 * TIMESLOT;
                timer_lst.add_timer(timer); 
            }            
            else if ((sockfd == pipefd[0]) && (events[i].events & EPOLLIN)) {
                int sig;
                char signals[1024];
                ret = recv(pipefd[0], signals, sizeof(signals), 0);
                if (ret == -1) {
                    //handle the errno
                    continue;
                }
                else if (ret == 0) {
                    continue;
                }
                else {
                    for (int i = 0; i < ret; i++) {
                        switch (signals[i]) {
                            case SIGALRM:
                                {
                                    timeout = 1;
                                    break;
                                }
                            case SIGTERM:
                                {
                                    stop_server = true;
                                }
                        }
                    }
                }
            }
            else if (events[i].events & EPOLLIN) {
                memset(users[sockfd].buf, 0, BUFFER_SIZE);
                ret = recv(sockfd, users[sockfd].buf, BUFFER_SIZE - 1, 0);
                cout << "get" << ret << "bytes of client data" << users[sockfd].buf << "from" << sockfd << endl;
                
                util_timer* timer = users[sockfd].timer;
                if (ret < 0) {
                    if (errno != EAGAIN) {
                        cb_func(&users[sockfd]);
                        if (timer) {
                            timer_lst.del_timer(timer);
                        }
                    }
                }
                else if (ret == 0) {
                    cb_func(&users[sockfd]);
                    if (timer) {
                        timer_lst.del_timer(timer);
                    }
                }
                else {
                    if (timer) {
                        time_t cur = time(NULL);
                        timer->expire = cur + 3 * TIMESLOT;

                        cout << "adjust timer once" << endl;
                        timer_lst.adjust_timer(timer);
                    }
                }
            }
        }
        if (timeout) {
            timer_handler();
            timeout = 0;
        }
    }
    close(listenfd);
    close(pipefd[1]);
    close(pipefd[0]);
    delete [] users;
    return 0;
}
