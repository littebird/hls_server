#ifndef SERVER_H
#define SERVER_H


#include<errno.h>
#include<sys/epoll.h>
#include<vector>

#include"connection.h"
#include"thread_pool.h"

class Server
{
public:
    Server();
    ~Server();
    void start_conn();
private:
    int  m_port;//端口号
    int listenfd;
    int epollfd;
    epoll_event *events;

    std::vector<Connection *> users;//用于保存所有连接进来的客户端

    std::shared_ptr<thread_pool> pool;
};

#endif // SERVER_H
