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
#include <boost/shared_ptr.hpp>
#include"connection.h"
#include"thread_pool.h"

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

    std::shared_ptr<thread_pool<Connection>> pool;
};

#endif // SERVER_H
