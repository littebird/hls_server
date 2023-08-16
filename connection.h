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
#include<QJsonDocument>
#include<QJsonObject>
#include<QByteArray>
#include<QFile>
#include "encoder.h"
#include "segmenter.h"

class Connection
{
public:
    Connection();
    static void process(Connection *conn_data);//处理请求和响应
    void close_conn();//关闭连接
    void init(int sockfd,const sockaddr_in &addr);//初始化新接收的连接
    static int m_epollfd;//所有的socket上的事件都被注册到同一个epoll上
    static const int READ_BUFFER_SIZE=4096;//读缓冲区大小
    static const int WRITE_BUFFER_SIZE=1024;//写缓冲区大小
    bool read();//非阻塞读
    bool write();//非阻塞写
    std::string _jsonStr;
private:
    char m_read_buf[READ_BUFFER_SIZE];
    char m_write_buf[WRITE_BUFFER_SIZE];
    int m_sockfd;//该http连接的socket
    sockaddr_in m_address; //通信的socket地址
    QFile m_file;//文件写操作
    std::string m_id;//视频id号
};

#endif // CONNECTION_H
