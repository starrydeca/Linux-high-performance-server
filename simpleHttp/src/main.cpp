#include "server.h"

int main(int argc, char** argv) {
    if (argc < 3) {
        cout << "usag:./simpleServer port path" << endl;
        return -1;
    }
    unsigned short port = atoi(argv[1]);
    //切换服务器的工作路径
    chdir(argv[2]);
    //初始化用于监听的套接字
    int lfd = initListenFd(port);
    //启动服务器
    epollRun(lfd);

    return 0;
}
