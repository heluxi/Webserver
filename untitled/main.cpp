#include <iostream>
#include<stdio.h>
#include<string.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<errno.h>
#include<fcntl.h>
#include<sys/epoll.h>
#include<threadpool.h>
#include<signal.h>
#include<http_conn.h>

#define MAX_FD 65535
#define MAX_EVENT_NUMBER 10000 //监听的最大的事件数量

//添加信号捕捉
void addsig(int sig,void(handler)(int))
{
    struct sigaction sa;
    memset(&sa,'\0',sizeof(sa));
    sa.sa_handler=handler;
    sigfillset(&sa.sa_mask);
    sigaction(sig,&sa,NULL);
}

//添加文件描述符到epoll中
extern  void  addfd(int epfd,int fd,bool one_shot);

//从epoll中删除文件描述
extern void  remove(int epfd,int fd);

//修改文件描述符
extern void modiffd(int epfd,int fd,int ev);

int main(int argc,char* argv[])
{

    if(argc<=1)
    {
        printf("按照如下格式运行：%s port\n",basename(argv[0]));
        exit(-1);
    }
    int port=atoi(argv[1]);

    //对SIGPIE信号进行处理 SIGPIPE 是在对一个已经关闭的写端的套接字进行写操作时产生的信号，通常会导致程序异常退出。通过调用 SIG_IGN 函数来忽略该信号，可以避免程序因此信号而异常退出。

    addsig(SIGPIPE,SIG_IGN);

    //创建线程池，初始化线程池 任务队列处理的数据就是http请求
    threadpool<http_conn> *pool=NULL;
    try{
        pool=new threadpool<http_conn>();
    }catch(...)
    {
        exit(-1);
    }
    //创建一个数组用于保存所有的客户端信息
    http_conn * users=new http_conn[MAX_FD];

    int lfd=socket(AF_INET,SOCK_STREAM,0);
    if(lfd==-1)
    {
        perror("socket()");
        exit(-1);
    }
    //设置端口复用,服务器终止后可以马上运行
    int reuse=1;
    setsockopt(lfd,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse));

    struct sockaddr_in ser_addr;
    ser_addr.sin_family=AF_INET;
    ser_addr.sin_port=htons(port);
    ser_addr.sin_addr.s_addr=INADDR_ANY;

    if(bind(lfd,(struct sockaddr*)&ser_addr,sizeof(ser_addr))==-1)
    {
        perror("bind()");
        exit(-1);
    }
    listen(lfd,5);

    //创建epooll对象，将监听套接字添加进去
    int epfd=epoll_create(5);
    epoll_event events[MAX_EVENT_NUMBER];

    addfd(epfd,lfd,false);

    //初始化http类中的epfd文件描述符
    http_conn::m_epfd=epfd;

    while(1)
    {
        int num=epoll_wait(epfd,events,MAX_EVENT_NUMBER,-1);
        if((num<0)&&(errno!=EINTR))
        {
            printf("epool failed\n");
            break;
        }
        //循环遍历事件数组
        for(int i=0;i<num;i++)
        {
            int curfd=events[i].data.fd;

            if(curfd==lfd)//有新的客户端连接
            {
                struct sockaddr_in cln_addr;
                socklen_t cln_addr_size=sizeof(cln_addr);
                int cfd=accept(lfd,(struct sockaddr*)&cln_addr,&cln_addr_size);
                if(cfd<0)
                {
                    perror("accept");
                    exit(-1);
                }
                if(http_conn::m_user_count>=MAX_FD)
                {
                    //目前服务器连接数满
                    //给客户端回写一个信息： 服务器繁忙
                    close(cfd);
                    continue;
                }
                //将新的客户端的数据初始化，放到数组中
                users[cfd].init(cfd,cln_addr);

            }else if(events[i].events& (EPOLLRDHUP | EPOLLHUP |EPOLLERR))//异常断开
            {
                //对方异常断开或错误
                users[curfd].close_conn();
            }else if(events[i].events & EPOLLIN){//读事件
                //一次性读出所有数据
                if(users[curfd].read())
                {
                    //加到线程池工作队列中
                    pool->append(users+curfd);
                }else{
                     users[curfd].close_conn();
                }

            }else if(events[i].events & EPOLLOUT)
            {
                if(!users[curfd].write())//一次性写完所有数据
                {

                }
            }

        }
    }
    close(epfd);
    close(lfd);
    delete [] users;
    delete pool;

    return 0;
}
