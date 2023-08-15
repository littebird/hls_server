#ifndef SERVER_H
#define SERVER_H

#include<iostream>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<errno.h>
#include<fcntl.h>
#include<sys/epoll.h>
#include<vector>
#include"connection.h"
class Server
{
public:
    Server();

    void start_conn();
private:
    int  m_port;//端口号
    int listenfd;
    int epollfd;
    epoll_event *events;

    std::vector<Connection *> users;//用于保存所有连接进来的客户端
};

#endif // SERVER_H
