#ifndef LOCKER_H
#define LOCKER_H
#include<iostream>
#include<pthread.h>
#include<exception>
#include<semaphore.h>

//线程同步机制封装类

//互斥锁类
class locker{

public:
    locker()
    {
        if(pthread_mutex_init(&m_mutex,NULL)!=0)
        {
            throw std::exception();
        }
    }
    ~locker()
    {
        pthread_mutex_destroy(&m_mutex);
    }
    bool lock()
    {
        return pthread_mutex_lock(&m_mutex);
    }
    bool unlock()
    {
        return pthread_mutex_unlock(&m_mutex);
    }
    pthread_mutex_t* get()
    {
        return &m_mutex;
    }
private:
    pthread_mutex_t m_mutex;//互斥锁


};

//条件变量类
class cond{
public:
    cond()
    {
        if(pthread_cond_init(&m_condtion,NULL)!=0)
        {
            throw std::exception();
        }
    }
    ~cond()
    {
        pthread_cond_destroy(&m_condtion);
    }
    bool wait(pthread_mutex_t *mutex)
    {
        return pthread_cond_wait(&m_condtion,mutex)==0;

    }
    bool timeWait(pthread_mutex_t *mutex,struct tiemspec t)
    {
        return pthread_cond_timedwait(&m_condtion,mutex,&t)==0;
    }
    bool siganl(pthread_mutex_t *mutex)
    {
        return pthread_cond_signal(&m_condtion)==0;
    }
    bool broadcast()
    {
        return pthread_cond_broadcast(&m_condtion)==0;
    }
private:
    pthread_cond_t m_condtion;
};

//信号量类

class sem{
public:
    sem()
    {
        if(sem_init(&m_sem,0,0)!=0)
        {
            throw std::exception();
        }
    }
    sem(int number)
    {
        if(sem_init(&m_sem,0,number)!=0)
        {
            throw std::exception();
        }
    }
    ~sem()
    {
        sem_destroy(&m_sem);
    }
    bool wait()
    {
        return sem_wait(&m_sem)==0;
    }
    bool post()
    {
        return sem_wait(&m_sem)==0;
    }
private:
    sem_t m_sem;
};

#endif
