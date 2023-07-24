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
        ~processpool() {
            delete[] m_sub_process;
        }
        void run();
};
