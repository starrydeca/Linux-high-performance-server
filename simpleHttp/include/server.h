#pragma once 
#include <iostream>
using namespace std;
#include <arpa/inet.h> 
#include <sys/epoll.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <assert.h>
#include <sys/sendfile.h>
#include <dirent.h> 

int initListenFd(unsigned short port);//初始化套接字

//启动epoll
int epollRun(int lfd);

//建立连接
int acceptClient(int lfd, int epfd);

//接收http请求
int recvHttpRequest(int cfd, int epfd);

//解析请求行
int parseRequestLine(const char* line, int cfd);

//发送文件
int sendFile(const char* fileName, int cfd);

//发送响应头（状态行+响应头）
int sendHeadMsg(int cfd, int status, const char* descr, const char* type, int length);
const char* getFileType(const char* name);

//发送目录
int sendDir(const char* dirName, int cfd);

//中文问题
int hexToDec(char c);
void decodeMsg(char* to, char* from);
