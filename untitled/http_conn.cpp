#include "http_conn.h"
#include<fcntl.h>

int http_conn::m_epfd=-1;

int http_conn:: m_user_count=0;//统计用户数量

void setnonblocking(int fd)
{
    int old_flag=fcntl(fd,F_GETFD);
    old_flag|=O_NONBLOCK;
    fcntl(fd,F_SETFL,old_flag);
}

//添加文件描述符到epoll中
void  addfd(int epfd,int fd,bool one_shot)
{
    epoll_event epev;
    epev.events=EPOLLIN|EPOLLRDHUP;
    epev.data.fd=fd;
    if(one_shot)
    {
        epev.events|=EPOLLONESHOT;
    }
    //设置文件描述符为非阻塞
    setnonblocking(fd);

    epoll_ctl(epfd,EPOLL_CTL_ADD,fd,&epev);



}
//从epoll中删除文件描述
void  remove(int epfd,int fd)
{
    epoll_ctl(epfd,EPOLL_CTL_DEL,fd,NULL);
    close(fd);
}

//修改文件描述符
void modiffd(int epfd,int fd,int ev)
{
    epoll_event epev;
    epev.events=ev|EPOLLONESHOT|EPOLLRDHUP;
    epev.data.fd=fd;

    epoll_ctl(epfd,EPOLL_CTL_MOD,fd,&epev);
}

http_conn::http_conn()
{

}

//由线程池中的工作线程调用，处理http请求的入口函数
void http_conn::process()
{
    //解析http请求
    HTTP_CODE read_ret=process_read();
    if(read_ret==NO_REQUEST)
    {
        modiffd(m_epfd,m_sockfd,EPOLLIN);
        return;
    }


    //生成响应

}

void http_conn::init(int cfd,sockaddr_in client_addr)
{
    m_sockfd=cfd;
    m_address=client_addr;

    //设置端口复用
    //端口复用
    int reuse=1;
    setsockopt(m_sockfd,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse));
    //添加到epool中
    addfd(m_epfd,m_sockfd,true);

}

void http_conn::close_conn()
{
    if(m_sockfd!=-1)
    {
        remove(m_epfd,m_sockfd);
        m_sockfd=-1;
        m_user_count--;//关闭一个连接 客户总数量-1
    }
}
//循环读取客户端数据 直到无数据可读
bool http_conn::read()
{
    if(m_read_index>=READ_BUFFER_SIZE)
    {
        return false;
    }
    //读的字节
    int bytes_read=0;
    while(true)
    {
        bytes_read=recv(m_sockfd,m_read_buf+m_read_index,READ_BUFFER_SIZE-m_read_index,0);
        if(bytes_read==-1)
        {
            if(errno==EAGAIN || errno==EWOULDBLOCK)
            {
                //没有数据
                break;
            }else{//出错
                return false;
            }
        }else if(bytes_read==0)//对方关闭连接
        {
            return false;
        }
        m_read_index+=bytes_read;
    }
    printf("读到了数据%s \n",m_read_buf);
    return true;
}

bool http_conn::write()
{

}

