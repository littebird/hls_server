#ifndef CONNECTION_H
#define CONNECTION_H

#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<sys/epoll.h>
#include<sys/types.h>
#include<fcntl.h>
#include<unistd.h>
#include<stdlib.h>
#include<sys/stat.h>
#include<sys/mman.h>
#include<errno.h>
#include<string.h>
#include<iostream>
class Connection
{
public:
    Connection();
    void process();//处理请求和响应
    void close_conn();//关闭连接
    void init(int sockfd,const sockaddr_in &addr);//初始化新接收的连接
    static int m_epollfd;//所有的socket上的事件都被注册到同一个epoll上
    static const int READ_BUFFER_SIZE=2048;//读缓冲区大小
    static const int WRITE_BUFFER_SIZE=1024;//写缓冲区大小
    bool read();//非阻塞读
    bool write();//非阻塞写
private:
    char m_read_buf[READ_BUFFER_SIZE];
    char m_write_buf[WRITE_BUFFER_SIZE];
    int m_sockfd;//该http连接的socket
    sockaddr_in m_address; //通信的socket地址
};

#endif // CONNECTION_H