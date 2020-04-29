#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include "../lock/locker.h"
#include "../CGImysql/sql_connection_pool.h"

template <typename T>
class threadpool
{
public:
    /*thread_number是线程池中线程的数量，max_requests是请求队列中最多允许的、等待处理的请求的数量 connPool是数据哭连接指针*/
    threadpool(connection_pool *connPool, int thread_number = 8, int max_request = 10000);
    ~threadpool();
    bool append(T *request);

private:
    /*工作线程运行的函数，它不断从工作队列中取出任务并执行之*/
    static void *worker(void *arg);
    void run();

private:
    int m_thread_number;        //线程池中的线程数
    int m_max_requests;         //请求队列中允许的最大请求数
    pthread_t *m_threads;       //描述线程池的数组，其大小为m_thread_number
    std::list<T *> m_workqueue; //请求队列
    locker m_queuelocker;       //保护请求队列的互斥锁
    sem m_queuestat;            //是否有任务需要处理
    bool m_stop;                //是否结束线程
    connection_pool *m_connPool;  //数据库
};

//线程构造函数
template <typename T>
threadpool<T>::threadpool( connection_pool *connPool, int thread_number, int max_requests) : m_thread_number(thread_number), m_max_requests(max_requests), m_stop(false), m_threads(NULL),m_connPool(connPool)
{
    if (thread_number <= 0 || max_requests <= 0)
        throw std::exception();
    //线程id初始化
    m_threads = new pthread_t[m_thread_number];
    if (!m_threads)
        throw std::exception();
    for (int i = 0; i < thread_number; ++i)
    {
        //printf("create the %dth thread\n",i);
        //循环创建线程,并将工作线程按要求进行运行
        if (pthread_create(m_threads + i, NULL, worker, this) != 0)
        {
            delete[] m_threads;
            throw std::exception();
        }
        //将线程进行分离后,不用单独对工作线程进行回收 将子线程的状态设置为detached,该线程运行结束后自动释放所有资源,否则
        //终止子线程的状态会一直保存,直到主线程调用pthread_join(threadid,NULL)获取线程的退出状态.
        if (pthread_detach(m_threads[i]))
        {
            delete[] m_threads;
            throw std::exception();
        }
    }
}

//线程析构函数
template <typename T>
threadpool<T>::~threadpool()
{
    delete[] m_threads;
    m_stop = true; //结束线程
}

//向请求队列中添加任务
//通过list容器创建请求队列,向队列中添加时,通过互斥锁保证线程安全,添加完成后通过信号量提醒有任务要处理,最后主要线程要同步
template <typename T>
bool threadpool<T>::append(T *request)
{
    m_queuelocker.lock();
    //根据硬件预先设置请求队列的最大值
    if (m_workqueue.size() > m_max_requests)
    {
        m_queuelocker.unlock();
        return false;
    }
    //添加任务
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    //信号量提醒有任务要处理
    m_queuestat.post();
    return true;
}
//内部访问私有成员函数run,完成线程处理要求
template <typename T>
void *threadpool<T>::worker(void *arg)
{
    //将参数强转为线程池类，调用成员方法
    threadpool *pool = (threadpool *)arg;
    pool->run();
    return pool;
}
//run执行任务
template <typename T>
void threadpool<T>::run()
{
    while (!m_stop)
    {
        //信号量等待
        m_queuestat.wait();
        m_queuelocker.lock();
        if (m_workqueue.empty()) //队列为空
        {
            m_queuelocker.unlock();
            continue;
        }
        //从请求队列中取出第一个任务
        //将任务从请求队列中删除
        T *request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();
        if (!request)
            continue;
        //从连接池中取出第一个数据库连接
        request->mysql = m_connPool->GetConnection();
        //process模板类的方法,这里是http类,进行处理
        request->process();
        //将数据库连接放回连接池
        m_connPool->ReleaseConnection(request->mysql);
    }
}


#endif // THREADPOOL_H
