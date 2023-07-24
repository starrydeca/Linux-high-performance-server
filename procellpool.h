#pragma once
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>

class process {
    public:
        process(): m_pid(-1){};
    public:
        pid_t m_pid;
        int m_pipefd[2];
};

template<typename T> 
class processpool {
    private:
        void setup_sig_pipe();
        void run_parent();
        void run_child();
    private:
        static const int MAX_PROCESS_NUMBER = 16;
        static const int USER_PER_PROCESS = 65535;
        static const int MAX_EVENT_NUMBER = 10000;
        int m_process_number;
        int m_idx;
        int m_epollfd;
        int m_listenfd;
        int m_stop;
        process* m_sub_process;
        static processpool<T>* m_instance;
    private:
        processpool(int listenfd, int process_number = 8);
    public:
        static processpool< T >* create(int listenfd, int process_number = 8) {
            if (!m_instance)    m_instance = new processpool< T >(listenfd, process_number);
            return m_instance;
        }
        ~processpool(){
            delete[] m_sub_process;
        } 
        void run(){
            if(m_idx != -1) {
                run_child();
                return ;
            }
            run_parent();
        }
};

static int sig_pipefd[2];

static int setnonblocking(int fd) {
    int flag = fcntl(fd, F_GETFL);
    flag |= O_NONBLOCK;
    flag = fcntl(fd, F_SETFL, flag);
    return flag;
}

static void addfd(int epollfd, int fd) {
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

static void removefd(int epollfd, int fd) {
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

static void sig_hangler(int sig) {
    int save_errno = errno;
    int msg = sig;
    send(sig_pipefd[1], (char*)&msg, 1, 0);
    errno = save_errno;
}

static void addsig(int sig, void(handler)(int), bool restart = true) {
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if (restart) {
        sa.sa_flags |= SA_RESTART;
    }
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL));
}
