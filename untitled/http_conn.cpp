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
    epev.events=EPOLLIN|EPOLLRDHUP;//EPOLLRDHUP：表示对方关闭了连接
    epev.data.fd=fd;
    if(one_shot)
    {
        epev.events|=EPOLLONESHOT;//EPOLLONESHOT：表示使用单次触发模式（One Shot），即只通知一次该事件发生，之后需要重新设置。保证一个socket在任何时刻只有一个线程操作
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
    if(read_ret==NO_REQUEST)//请求不完整,还要继续获取客户端的数据
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
    m_user_count++;//总用户数+1

    init();

}

void http_conn::init()
{
    m_check_state = CHECK_STATE_REQUESTLINE;    // 初始状态为检查请求行
    m_start_line = 0;
    m_checked_index = 0;
    m_read_index = 0;
    m_method=GET;
    m_url=0;
    m_version=0;
    m_linger=false;

    bzero(m_read_buf,READ_BUFFER_SIZE);

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
//循环读取客户端数据 直到无数据可读或者对方关闭连接
bool http_conn::read()
{
    if(m_read_index>=READ_BUFFER_SIZE)//缓冲满
    {
        return false;
    }
    //读的字节
    int bytes_read=0;
    while(true)
    {
        bytes_read=recv(m_sockfd,m_read_buf+m_read_index,READ_BUFFER_SIZE-m_read_index,0);//每次从上一次读取的位置的下一个开始读
        if(bytes_read==-1)
        {
            if(errno==EAGAIN || errno==EWOULDBLOCK)//使用非阻塞IO时,如果当前没有数据可读 会将errno设置为EAGIN或EWODBLOCK
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

//主状态机 从大的范围解析http请求
http_conn::HTTP_CODE http_conn::process_read()
{

    LINE_STATUS line_status=LINE_OK;
    HTTP_CODE ret=NO_REQUEST;

    char *text=0;

    while(((m_check_state==CHECK_STATE_CONTENT) && (line_status==LINE_OK))||((line_status=parse_line())==LINE_OK))
    {
        //解析到了一行完整的数据 或者解析到了请求体也是完成的数据
        //获取一行数据
        text=get_line();
        m_start_line=m_checked_index;
        printf("got 1 http line:%s\n",text);

        switch (m_check_state) {
        case CHECK_STATE_REQUESTLINE://请求首行
        {
            ret=parse_request_line(text);
            if(ret==BAD_REQUEST)
            {
                return BAD_REQUEST;
            }
            break;
        }
        case CHECK_STATE_HEADER://请求头
        {
            ret=parse_headers(text);
            if(ret==BAD_REQUEST)
            {
                return BAD_REQUEST;
            }else if(ret==GET_REQUEST)
            {
                return do_request();
            }
            break;
        }
        case CHECK_STATE_CONTENT://请求体
        {
            ret=parse_content(text);
            if(ret==GET_REQUEST)
            {
                return do_request();
            }
            line_status=LINE_OPEN;
            break;
        }
        default:{
            return INTERNAL_ERROR;
        }
        }
    }

    return NO_REQUEST;
}

//解析请求行 获得请求行 目标Url http协议版本
http_conn::HTTP_CODE http_conn::parse_request_line(char *text)
{

    //GET /index.html HTTP1.1
    m_url=strpbrk(text," \t");
    *m_url++='\0';

    char *method=text;
    if(strcasecmp(method,"GET")==0)
    {
        m_method=GET;
    }else{
        return BAD_REQUEST;
    }
    m_version=strpbrk(m_url," \t");
    if(!m_version)
    {
        return BAD_REQUEST;
    }
    *m_version++='\0';
    if(strcasecmp(m_version,"HTTP/1.1")!=0)
    {
        return BAD_REQUEST;
    }
    if(strncasecmp(m_url,"http://",7)==0)
    {
        m_url+=7;
        m_url=strchr(m_url,'/');
    }
    if(!m_url||m_url[0]!='/')
    {
        return BAD_REQUEST;
    }

    m_check_state=CHECK_STATE_HEADER;//主状态机检查状态变成检查请求头

    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::parse_headers(char *text)
{

    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::parse_content(char *text)
{

    return NO_REQUEST;
}

//解析一行 判断依据\r\n
http_conn::LINE_STATUS http_conn::parse_line()
{

    char temp;
    for(;m_checked_index<m_read_index;++m_checked_index)
    {
        temp=m_read_buf[m_checked_index];//一个字符一个字符的解析正在读取的数据
        if(temp=='\r')
        {
            if((m_checked_index+1)==m_read_index)//读取的数据不完整
            {
                return LINE_OPEN;
            }else if(m_read_buf[m_checked_index+1]=='\n')//读取到了\r\n 表示读取到了完整的一行
            {
                m_read_buf[m_checked_index++]='\0';//将读取到\r\n修改为\0结束符号
                m_read_buf[m_checked_index++]='\0';
                return LINE_OK;
            }

            return LINE_BAD;
        }else if(temp=='\n')
        {
            if((m_checked_index>1)&&(m_read_buf[m_checked_index-1])=='\r')//前一个字符为\r 此时也是一行数据
            {
                m_read_buf[m_checked_index]='\0';
                m_read_buf[m_checked_index++]='\0';
                return LINE_OK;
            }
        }
        return LINE_OPEN;
    }

    return LINE_OK;
}

http_conn::HTTP_CODE http_conn::do_request()
{

}

