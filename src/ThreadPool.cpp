#include "ThreadPool.h"
#include "Queue.h"

namespace chat {

/* 线程类的构造函数
 * 调用该构造函数，会创建一个线程，并使该线程运行在线程函数内
 *
 * @parm func 线程函数
 * @parm pool 线程池指针，指向线程所在的线程池
 */
Thread::Thread(ThreadFunc func, ThreadPool* pool)
    : func_(func) 
{
    pthread_create(&tid_, NULL, func_, (void*)pool);    
}

/* 线程池的构造函数
 * 设定线程池中线程的数目
 *
 * @parm numThreads 线程池中线程数目
 */
ThreadPool::ThreadPool(int numThreads)
    : numThreads_(numThreads)
{
    threads_.reserve(numThreads);
}

/* 线程池的析构函数
 * 对线程池中每个线程所分配的内存进行释放
 */
ThreadPool::~ThreadPool()
{
    for (int i = 0; i < numThreads_; i++)
        delete threads_[i];
}

/* 线程池的启动函数
 * 创建指定数目的线程，为每个线程设定线程函数和线程函数的参数
 *
 * @parm void
 * @return void
 */
void ThreadPool::run()
{
    for (int i = 0; i < numThreads_; i++) {
        Thread* thread = new Thread(threadFunc, this);
        threads_.push_back(thread);
    }
}

/* static void* threadFunc(void* arg);
 *
 * 线程池中每个线程的线程函数
 *
 * @parm arg 线程函数的参数
 * @return void* 线程终止状态
 */
void* ThreadPool::threadFunc(void* arg)
{
    while (true) {
        ThreadPool* pool = (ThreadPool*)arg; 

        // 线程通过线程池类提供的接口得到工作队列，
        // 进而从工作队列中取出任务
        WorkType work;
        (pool->getWorkQueue())->get(work);

        // 执行从工作队列中取得的任务
        work();
    }

    return (void*)0;
}

/* 线程池对外提供的获取工作队列的接口
 * 线程池中的线程可以通过该接口取任务
 *
 * @parm void
 * @return Queue<WorkType>* 指向工作队列的指针
 */
Queue<WorkType>* ThreadPool::getWorkQueue()
{
    return &workQueue_;
}

/* 线程池对外提供的获取线程数组的接口
 *
 * @parm void
 * @return vector<Thread>* 指向线程数组的指针
 */
std::vector<Thread*>* ThreadPool::getThreads()
{
    return &threads_;
}

} // namespace chat
