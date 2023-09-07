#include<threadpool.h>
#include<cstdio>

template<typename T>
threadpool<T>::threadpool(int thread_number, int max_rquests):m_thread_number(thread_number),m_max_requests(m_max_requests),m_stop(false),m_threads(NULL)
{
    if(thread_number<=0||(max_rquests<=0))
    {
        throw std::exception();
    }
    m_threads=new pthread_t[m_thread_number];
    if(!m_threads)
    {
        throw std::exception();
    }
    //创建thread_number个线程，并将它们设置为线程脱离
    for(int i=0;i<m_thread_number;i++)
    {
        if(pthread_create(m_threads+i,NULL,worker,this)==-1)
        {
            delete [] m_threads;
            throw std::exception();
        }

        if(pthread_detach(m_threads[i]))
        {
            delete [] m_threads;
            throw std::exception();
        }
    }

}

template<typename T>
threadpool<T>::~threadpool()
{
    delete [] m_threads;
    m_stop=true;
}

//相当于生产者
template<typename T>
bool threadpool<T>::append(T *request)
{
    m_queuelocker.lock();
    if(m_workqueue.size()>m_max_requests)
    {
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();

    return true;
}

//从工作队列中取数据 消费者
template<typename T>
void threadpool<T>::run()
{
    while (!m_stop) {
        m_queuestat.wait();
        m_queuelocker.lock();
        if(m_workqueue.empty())
        {
            m_queuelocker.unlock();
            continue;
        }
        T * request=m_workqueue.front();//从任务队列中取出一个任务执行
        m_workqueue.pop_front();
        m_queuelocker.unlock();

        if(!request)
        {
            continue;
        }
        request->prcess();
    }
}

template<typename T>
void *threadpool<T>::worker(void *arg)//每个工作者线程实际执行的是run函数
{
    threadpool* pool=(threadpool*)arg;
    pool->run();
    return pool;
}
