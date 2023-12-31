#include "server.h"


extern void removefd(int epollfd, int fd);
extern void addfd(int epollfd, int fd, bool one_shot);
extern void modfd(int epollfd, int fd, int ev);

Server::Server()
    :m_port{9999},
     users{std::vector<Connection *>(10000,new Connection())},
     pool{new thread_pool()}
{
     //创建监听的套接字
     listenfd=socket(PF_INET,SOCK_STREAM,0);

     //设置端口复用
     int reuse=1;
     setsockopt(listenfd,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse));

     //绑定
     struct sockaddr_in add_ress;
     add_ress.sin_family=AF_INET;
     add_ress.sin_addr.s_addr=inet_addr("10.252.154.131");
     add_ress.sin_port=htons(m_port);
     bind(listenfd,(struct sockaddr *)&add_ress,sizeof(add_ress));

     //监听
     listen(listenfd,10);

     //创建epoll对象
     events=new epoll_event[10000];
     epollfd=epoll_create1(0);

     //将监听的文件描述符添加到epoll对象中
     addfd(epollfd,listenfd,false);
     Connection::m_epollfd=epollfd;

}

Server::~Server()
{
    delete []events;
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
                std::cout<<"clie connect me"<<std::endl;
            }
            else if(events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
            {
                //异常断开或错误事件
                std::cout<<"Exception close connection fd: "<<sockfd<<std::endl;
                users[sockfd]->close_conn();
            }
            else if(events[i].events & EPOLLIN)
            {

                if(users[sockfd]->read())
                {
                    if(*(users[sockfd]->_jsonStr.end()-1)=='\r'){//检查来自对端的数据，以\r为结束符
                        std::function<void()> task=std::bind(&Connection::process,this->users[sockfd]);
                        pool->submit(task);
                    }

                }
                else
                {
                    std::cout<<"close connection"<<std::endl;
                    users[sockfd]->close_conn();
                }
            }
            else if(events[i].events & EPOLLOUT)
            {
                if(!users[sockfd]->write())
                {
                    std::cout<<"close connection write error"<<std::endl;
                    users[sockfd]->close_conn();
                }
            }
        }
    }
}
