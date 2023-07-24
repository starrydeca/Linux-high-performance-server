#include "procellpool.h"
#include <iostream>
using namespace std;
template< typename T >
processpool< T >* processpool< T >::m_instance = nullptr;

template< typename T >
processpool< T >::processpool(int listenfd, int process_number)
    :m_listenfd(listenfd), m_process_number(process_number), m_idx(-1), m_stop(false) {
    assert((process_number > 0) && (process_number <= MAX_PROCESS_NUMBER));
    m_sub_process = new process[process_number];
    assert(m_sub_process);
    
    for (int i = 0; i < process_number; ++i) {
        int ret = socketpair(PF_UNIX, SOCK_STREAM, 0, m_sub_process[1].m_pipefd);
        assert(ret == 0);

        ret = m_sub_process[i].m_pid = fork();
        assert(ret >= 0);
        if (ret > 0) {
            close(m_sub_process[i].m_pipefd[1]);
            continue;
        }
        else {
            close(m_sub_process[i].m_pipefd[0]);
            m_idx = i;
            break;
        }
    }    
}


template< typename T >
void processpool< T >::setup_sig_pipe() {
    m_epollfd = epoll_create(5);
    assert(m_epollfd != -1);

    int ret = socketpair(PF_UNIX, SOCK_STREAM, 0, sig_pipefd); 
    assert(ret != -1);

    setnonblocking(sig_pipefd[1]);
    addfd(m_epollfd, sig_pipefd[0]);

    addsig(SIGCHLD, sig_hangler);
    addsig(SIGTERM, sig_hangler);
    addsig(SIGINT, sig_hangler);
    addsig(SIGPIPE, SIG_IGN);
}

template< typename T >
void processpool< T >::run_child() {
    setup_sig_pipe();
    int pipefd = m_sub_process[m_idx].m_pipefd[1];
    addfd(m_epollfd, pipefd);

    epoll_event events[MAX_PROCESS_NUMBER];
    T* users = new T[USER_PER_PROCESS];
    assert(users);
    int number = 0;
    int ret = -1;

    while(! m_stop) {
        number = epoll_wait(m_epollfd, events, MAX_PROCESS_NUMBER, -1);
        if ((number < 0) && (errno != EINTR)) {
            cout << "epoll failure" << endl;
            break;
        }

        for  (int i = 0; i < number; i++) {
            int sockfd = events[i].data.fd;
            if ((sockfd == pipefd) && (events[i].events & EPOLLIN)) {
                int client = 0;
                ret = recv(sockfd, (char*)&client, sizeof(client), 0);
                if (((ret < 0) && (errno != EAGAIN)) || ret == 0)   continue;
                else {
                    struct sockaddr_in client_address;
                    socklen_t client_addrlength = sizeof(client_address);
                    int connfd = accept(m_listenfd, (sockaddr*)&client_address, &client_addrlength);
                    if (connfd < 0) {
                        cout << "errno is:" << errno << endl;
                        continue;
                    }
                    addfd(m_epollfd, connfd);
                    users[connfd].init(m_epollfd, connfd, client_address);
                }
            }
            else if ((sockfd == sig_pipefd[0]) && (events[i].events & EPOLLIN)) {
                int sig;
                char signals[1024];
                ret = recv(sig_pipefd[0], signals, sizeof(signals), 0);
                if (ret <= 0)   continue;
                else {
                    for (int i = 0; i < ret; i++) {
                        switch (signals[i]) {
                            case SIGCHLD:
                                {
                                    pid_t pid;
                                    int stat;
                                    while ((pid = waitpid(-1, &stat, WNOHANG)) > 0) continue;
                                    break;
                                }
                            case SIGTERM:
                            case SIGINT:
                                {
                                    m_stop = true;
                                    break;
                                }
                            default:
                                break;
                        }
                    }
                }
            }
            else if (events[i].events & EPOLLIN) {
                users[sockfd].process();
            }   
            else    continue;
        }
    }

    delete[] users;
    users = nullptr;
    close(pipefd);
    close(m_epollfd);
}

template<typename T >
void processpool< T >::run_parent() {
    setup_sig_pipe();
    addfd(m_epollfd, m_listenfd);

    epoll_event events[MAX_PROCESS_NUMBER];
    int sub_process_counter = 0;
    int new_conn = 1;
    int number = 0;
    int ret = -1;

    while (!m_stop) {
        number = epoll_wait(m_epollfd, events, MAX_PROCESS_NUMBER, -1);
        if ((number < 0) && (errno != EINTR)) {
            cout << "epoll failure" << endl;
            break;
        }
        for (int i = 0; i < number; i++) {
            int sockfd = events[i].data.fd;
            if (sockfd == m_listenfd) {
                int i = sub_process_counter;
                do {
                    if (m_sub_process[i].m_pid != -1)   break;
                    i = (i + 1) % m_process_number;
                }while (i != sub_process_counter);

                if (m_sub_process[i].m_pid == -1) {
                    m_stop = true;
                    break;
                }
                sub_process_counter = (i + 1) % m_process_number;
                send(m_sub_process[i].m_pipefd[0], (char*)&new_conn, sizeof(new_conn), 0);
                cout << "send request to child" << i << endl;
            }
            else if (sockfd == sig_pipefd[0] && (events[i].events & EPOLLIN)) {
                int sig;
                char signals[1024];
                ret = recv(sig_pipefd[0], signals, sizeof(signals), 0);
                if (ret <= 0)   continue;
                else {
                    for (int i = 0; i < ret; i++) {
                        switch (signals[i]) {
                            case SIGCHLD:
                                {
                                    pid_t pid;
                                    int stat;
                                    while ((pid = waitpid(-1, &stat, WNOHANG)) > 0) {
                                        for (int i = 0; i < m_process_number; i++) {
                                            if (m_sub_process[i].m_pid == pid) {
                                                cout << "child" << i << "join" << endl;
                                                close(m_sub_process[i].m_pipefd[0]);
                                                m_sub_process[i].m_pid = -1;
                                            }
                                        }
                                    }
                                    m_stop = true;
                                    for (int i = 0; i < m_process_number; i++) {
                                        if (m_sub_process[i].m_pid != -1)   m_stop = false;
                                    }
                                    break;
                                }
                            case SIGTERM:
                            case SIGINT:
                                {
                                    cout << "kill all the child now" << endl;
                                    for (int i = 0; i < m_process_number; i++) {
                                        int pid = m_sub_process[i].m_pid;
                                        if (pid != -1)  kill(pid, SIGTERM);
                                    }
                                    break;
                                }
                            default:
                                break;
                        }
                    }
                }
            }
            else continue;
        }
    }
    close(m_epollfd);
}
