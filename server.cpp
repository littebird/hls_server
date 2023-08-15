#include "server.h"


extern void removefd(int epollfd, int fd);
extern void addfd(int epollfd, int fd, bool one_shot);
extern void modfd(int epollfd, int fd, int ev);
Server::Server()
    :m_port{9999},
     users{std::vector<Connection *>(10000,new Connection())}
{
     //创建监听的套接字
     listenfd=socket(PF_INET,SOCK_STREAM,0);

     //设置端口复用
     int reuse=1;
     setsockopt(listenfd,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse));

     //绑定
     struct sockaddr_in add_ress;
     add_ress.sin_family=AF_INET;
     add_ress.sin_addr.s_addr=inet_addr("127.0.0.1");
     add_ress.sin_port=htons(m_port);
     bind(listenfd,(struct sockaddr *)&add_ress,sizeof(add_ress));

     //监听
     listen(listenfd,10);

     //创建epoll对象
     events=new epoll_event[10000];
     epollfd=epoll_create(10);

     //将监听的文件描述符添加到epoll对象中
     addfd(epollfd,listenfd,false);
     Connection::m_epollfd=epollfd;

     start_conn();
}

void Server::start_conn()
{
    while(true)
    {
        int num=epoll_wait(epollfd,events,10000,-1);
        if(num<0&&(errno!=EINTR))
        {
            std::cout<<"epoll failure"<<std::endl;
            break;
        }

        for(int i=0;i<num;++i)
        {
            int sockfd=events[i].data.fd;
            if(sockfd==listenfd)
            {
                //有客户端连接
                struct sockaddr_in client_address;
                socklen_t  client_addrlen=sizeof(client_address);
                int connfd=accept(listenfd,(struct sockaddr*)&client_address,&client_addrlen);

                //将新的客户数据初始化，放到数组中
                users[connfd]->init(connfd,client_address);

            }
            else if(events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
            {
                //异常断开或错误事件
                std::cout<<"Exception close connection:"<<sockfd<<std::endl;
                users[sockfd]->close_conn();
            }
            else if(events[i].events & EPOLLIN)
            {
                if(users[sockfd]->read())
                {
                    //一次性把所有数据读完
//                    pool->append(&userss[sockfd].user_conn);
                }
                else
                {
                    std::cout<<"close connection2"<<std::endl;
                    users[sockfd]->close_conn();
                }
            }
            else if(events[i].events & EPOLLOUT)
            {
                if(!users[sockfd]->write())
                {
                    std::cout<<"close connection3"<<std::endl;
                    users[sockfd]->close_conn();
                }
            }
        }
    }
}
