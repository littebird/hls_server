#include "connection.h"
Connection::Connection()
    :m_sockfd{-1}
{

}

int Connection::m_epollfd=-1;
void setnonblocking(int fd)
{
    int old_flag=fcntl(fd,F_GETFL);
    int new_flag=old_flag | O_NONBLOCK;
    fcntl(fd,F_SETFL,new_flag);

}
void addfd(int epollfd, int fd, bool one_shot)
{
    epoll_event event;
    event.data.fd=fd;
    event.events=EPOLLIN | EPOLLRDHUP ;
    if(one_shot)
    {
        event.events |= EPOLLONESHOT;
    }
    epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&event);

    //设置文件描述符非阻塞
    setnonblocking(fd);
}

void removefd(int epollfd, int fd)
{
    epoll_ctl(epollfd,EPOLL_CTL_DEL,fd,0);
    close(fd);//关闭文件描述符

}

void modfd(int epollfd, int fd, int ev)
{
    epoll_event event;
    event.data.fd=fd;
    event.events=ev | EPOLLONESHOT | EPOLLRDHUP;
    epoll_ctl(epollfd,EPOLL_CTL_MOD,fd,&event);
}

void Connection::init(int sockfd, const sockaddr_in &addr)
{
    m_sockfd=sockfd;
    m_address=addr;

    //端口复用
    int reuse=1;

    setsockopt(m_sockfd,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse));

    //添加到epoll对象中
    addfd(m_epollfd,sockfd,true);
}


void Connection::close_conn()
{
    if(m_sockfd!=-1)
    {
        removefd(m_epollfd,m_sockfd);
        m_sockfd=-1;
        memset(m_write_buf,'\0',sizeof(m_write_buf));
        memset(m_read_buf,'\0',sizeof(m_read_buf));

    }
}

bool Connection::read()
{
    //读取所有数据
    int bytes_read=0;
    memset(m_read_buf,'\0',sizeof(m_read_buf));

    while(true)
    {

        bytes_read=recv(m_sockfd,m_read_buf,READ_BUFFER_SIZE,0);
        std::cout<<bytes_read<<std::endl;

        if(bytes_read==-1)
        {
            if(errno==EAGAIN || errno==EWOULDBLOCK)
            {
                //没有数据
                break;
            }
            return false;
        }
        else if(bytes_read==0)
        {
            //对方关闭连接
            return false;
        }
        else
        {
            m_jsonStr.append(m_read_buf);
            break;
        }
    }
    return true;
}

bool Connection::write()
{
    memset(m_write_buf,'\0',sizeof(m_write_buf));

    int ret=send(m_sockfd,m_write_buf,WRITE_BUFFER_SIZE,0);

    if(ret<=-1)
    {
         if(errno==EAGAIN)
          {
              modfd(m_epollfd,m_sockfd,EPOLLOUT);
              return true;
          }
         else
         {
             std::cout<<"send failure"<<std::endl;
             return false;
         }
    }
    else if(ret>0)
    {
         std::cout<<"send success"<<std::endl;
         modfd(m_epollfd,m_sockfd,EPOLLIN);
         return true;
    }
}

void Connection::process(Connection *conn_data)
{

    //线程处理发过来的数据
//    QJsonDocument doc(QJsonDocument::fromJson(QByteArray(conn_data->m_jsonStr.c_str())));
//    QJsonObject obj=doc.object();
//    std::string id=obj.value("id").toString().toStdString();
//    std::string file_name=id+"."+obj.value("postfix").toString().toStdString();
//    std::string data=obj.value("data").toString().toStdString();

    //视频数据写入到文件
//     conn_data->m_file.open("../hls_server/resource/"+file_name,std::ios::out|std::ios::app);
//     conn_data->m_file<<data;//数据写入文件
//     conn_data->m_file.close();//关闭文件

//     std::string full_path="../hls_server/resource/"+file_name;
//     //处理接收到的数据
//     Encoder encoder;
//     encoder.VOD(full_path.c_str());//转码播放
//     //to do 切片生成m3u8文件

//     segmenter seg{"../hls_server/tests.ts"};
//     seg.init();
//     seg.slice();

//     //处理将要写的数据
//    std::string send_data="http://10.252.154.131/hls/seg.m3u8";

//    strcpy(conn_data->m_write_buf,send_data.data());

//    modfd(m_epollfd,conn_data->m_sockfd,EPOLLOUT);

}

