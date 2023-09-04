#ifndef THREADPOOL
#define THREADPOOL

#include<pthread.h>
#include<list>
#include<locker.h>

//线程池类，定义成模板类是为了代码复用

template<typename T>
class threadpool{
public:
    threadpool(int thread_number=8,int max_rquests=10000);
    ~threadpool();
    //添加任务
    bool append(T* request);
    //从工作队列中取数据
    void run();
private:
    static void* worker(void* arg);


    //线程数量
    int m_thread_number;

    //线程池数组，大小为m_thread_number
    pthread_t* m_threads;

    //请求队列中最多允许的，等待处理的请求数量
    int m_max_requests;

    //请求队列

    std::list<T*> m_workqueue;

    //互斥锁
    locker m_queuelocker;

    //信号量
    sem m_queuestat;
    //是否结束线程

    bool m_stop;

};

#endif
