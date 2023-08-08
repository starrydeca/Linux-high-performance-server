#include "server.h"

int initListenFd(unsigned short port) {
    //1.创建监听的fd
    int lfd = socket(PF_INET, SOCK_STREAM, 0);
    if (lfd == -1) {
        perror("socket");
        return -1;
    }
    //2.设置端口复用
    int opt = 1;
    int ret = setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if (ret == -1) {
        perror("setsockopt");
        return -1;
    }
    //3.绑定
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    ret = bind(lfd, (sockaddr*)&addr, sizeof addr);
    if (ret == -1) {
        perror("bind");
        return -1;
    }
    //4.设置监听
    ret = listen(lfd, 128);
    if (ret == -1) {
        perror("listen");
        return -1;
    }
    //返回fd
    return lfd;
}

int epollRun(int lfd) {
    //1.创建epoll实例
    int epfd = epoll_create(1);
    if (epfd == -1) {
        perror("epoll_create");
        return -1;
    }
    //2.lfd 上树
    epoll_event ev;
    ev.data.fd = lfd;
    ev.events = EPOLLIN;
    int ret = epoll_ctl(epfd, EPOLL_CTL_ADD, lfd, &ev);
    if (ret == -1) {
        perror("epoll_ctl");
        return -1;
    }
    //3.检测
    epoll_event evs[1024];

    int evSize = sizeof(evs)/sizeof(epoll_event);
    while (1) {
        /*epoll_wait
            1.设置为0,非阻塞，epoll_wait没有检测到就返回0,表示0个连接。
            2.设置为-1,阻塞，epoll_wait没有检测到就不返回，直到检测到。
        */
        int num = epoll_wait(epfd, evs, evSize, -1);
        for (int i = 0; i < num; i++) {
            int fd = evs[i].data.fd;
            if (fd == lfd) {//用于监听的描述符，证明有新连接来了
                //建立新连接
                acceptClient(lfd, epfd);
            } else {
                //主要是接收对端的数据
                recvHttpRequest(fd, epfd);
            }
        }
    }
}

int acceptClient(int lfd, int epfd) {
    //1.建立连接
    int cfd = accept(lfd, NULL, NULL);
    if (cfd == -1) {
        perror("accept");
        return -1;
    }
    //2.设置非阻塞
    int flag = fcntl(cfd, F_GETFL);
    flag |= O_NONBLOCK;
    fcntl(cfd, F_SETFL, flag);

    //3.cfd添加到epoll中
    epoll_event ev;
    ev.data.fd = cfd;
    ev.events = EPOLLIN | EPOLLET;
    int ret = epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &ev);
    if (ret == -1) {
        perror("epoll_ctl");
        return -1;
    }
    return 0;
}

int recvHttpRequest(int cfd, int epfd) {
    cout << "开始接收数据了...." << endl;
    char buf[4096] = {0};
    char tmp[1024] = {0};
    int len = 0, totle = 0;
    while ((len = recv(cfd, tmp, sizeof tmp, 0)) > 0) {
        if (totle + len < sizeof buf) {
            memcpy(buf + totle, tmp, len);
        }
        totle += len;
    }
    //判断数据是否被接收完毕
    if (len == -1 && errno == EAGAIN) {
        //解析请求行
        char* pt = strstr(buf, "\r\n");
        int reqLen = pt - buf;
        buf[reqLen] = '\0';
        parseRequestLine(buf, cfd);
    } else if (len == 0) {
        //客户端断开了连接
        epoll_ctl(epfd, EPOLL_CTL_DEL, cfd, NULL);
        close(cfd);
    } else perror("recv");
    return 0;
}

int parseRequestLine(const char* line, int cfd) {
    //解析请求行
    char method[12];
    char path[1024];
    sscanf(line, "%[^ ] %[^ ]", method, path);
    cout << "method:" << method << "path:" << path << endl;
    if (strcasecmp(method, "get") != 0) return -1;
    //处理客户端请求的静态资源（目录或者文件)
    char* file = NULL;
    if (strcmp(path, "/") == 0) {
        file = "./";
    } else {
        file = path + 1;
    }
    //获取文件属性
    struct stat st;
    int ret = stat(file, &st);
    if (ret == -1) {
        //文件不存在，回复404
        sendHeadMsg(cfd, 404, "Not Found", getFileType(".html"), -1);
        sendFile("404.html", cfd);
        return 0;
    }
    //判断文件类型
    if (S_ISDIR(st.st_mode)) {
        //把这个目录中的内容发送给客户端
        sendHeadMsg(cfd, 200, "OK", getFileType(".html"), -1);
        sendDir(file, cfd);
    } else {
        //把文件的内容发送给客户端
        sendHeadMsg(cfd, 200, "OK", getFileType(file), st.st_size);
        sendFile(file, cfd);
    }
    return 0;
}

int sendFile(const char* fileName, int cfd) {
    //1.打开文件
    int fd = open(fileName, O_RDONLY);
    assert(fd > 0);
#if 1
    while (1) {
        char buf[1024];
        int len = read(fd, buf, sizeof buf);
        if (len > 0) {
            send(cfd, buf, len, 0);
            usleep(10);//休眠1微秒
        } else if (len == 0)  break;
        else perror("send");
    }
#else
    int fileSize = lseek(fd, 0, SEEK_END);
    sendfile(cfd, fd, NULL, fileSize);
#endif
    close(fd);
    return 0;
}

int sendHeadMsg(int cfd, int status, const char* descr, const char* type, int length) {
    //状态行
    char buf[4096] = {0};
    sprintf(buf, "http/1.1 %d %s\r\n", status, descr);
    //响应头
    sprintf(buf + strlen(buf), "content-type: %s\r\n", type);
    sprintf(buf + strlen(buf), "content-length: %d\r\n\r\n", length);

    send(cfd, buf, strlen(buf), 0);
    return 0;
}

const char* getFileType(const char* name) {
    // a.jpg a.mp4 a.hyml
    // 自右向左查找 '.'字符，如果不存在返回NULL
    const char* dot = strrchr(name, '.');
    if (dot == NULL)
        return "text/plain; charset=utf-8";//纯文本
    if (strcmp(dot, ".html") == 0 || strcmp(dot, ".htm") == 0)
        return "text/html; charset=utf-8";
    if (strcmp(dot, ".jpg") == 0 || strcmp(dot, ".jpeg") == 0)
        return "image/jpeg";
    if (strcmp(dot, ".gif") == 0)
        return "image/jpeg";
    if (strcmp(dot, ".png") == 0)
        return "image/png";
    if (strcmp(dot, ".css") == 0)
        return "text/css";
    if (strcmp(dot, ".au") == 0)
        return "audio/basic";
    if (strcmp(dot, ".wav") == 0)
        return "audio/wav";
    if (strcmp(dot, ".avi") == 0)
        return "video/x-msvideo";
    if (strcmp(dot, ".mov") == 0 || strcmp(dot, ".qt") == 0)
        return "video/quicktime";
    if (strcmp(dot, ".mpeg") == 0 || strcmp(dot, ".mpe") == 0)
        return "video/mpeg";
    if (strcmp(dot, ".vrml") == 0 || strcmp(dot, ".wrl") == 0)
        return "model/vrml";
    if (strcmp(dot, ".midi") == 0 || strcmp(dot, ".mid") == 0)
        return "model/midi";
    if (strcmp(dot, ".mp3") == 0)
        return "audio/mpeg";
    if (strcmp(dot, ".ogg") == 0)
        return "application/ogg";
    if (strcmp(dot, ".pac") == 0)
        return "application/x-ns-proxy-autoconfig";

    return "text/plain; chahrset=utf-8";
}

int sendDir(const char* dirName, int cfd) {
    char buf[4096] = {0};
    sprintf(buf, "<html><head><title>%s</title></head><body><table>", dirName);

    struct dirent** namelist;
    int num = scandir(dirName, &namelist, NULL, alphasort);
    for (int i = 0; i < num; i++) {
        //取出文件名、
        char* name = namelist[i]->d_name;
        struct stat st;
        char subPath[1024] = {0};
        sprintf(subPath, "%s/%s", dirName, name);
        stat(subPath, &st);
        if (S_ISDIR(st.st_mode)) {
            sprintf(buf + strlen(buf), "<tr><td><a href=\"%s/\">%s</a></td><td>%ld</td></tr>", name, name, st.st_size);
        } else {
            sprintf(buf + strlen(buf), "<tr><td><a href=\"%s\">%s</a></td><td>%ld</td></tr>", name, name, st.st_size);
        }
        send(cfd, buf, strlen(buf), 0);
        memset(buf, 0, sizeof buf);
        free(namelist[i]);
    }
    sprintf(buf, "</table></body></html>");
    send(cfd, buf, strlen(buf), 0);
    free(namelist);
    return 0;
}
