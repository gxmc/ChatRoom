/* 线程池的实现
 *
 */

#ifndef _CHATROOM_SRC_THREADPOOL_H_
#define _CHATROOM_SRC_THREADPOOL_H_

#include <vector>
#include <functional>

#include "Queue.h"

namespace chat {

typedef void* (*ThreadFunc)(void*);

typedef std::function<void()> WorkType;

class ThreadPool;

class Thread {
public:
    Thread(ThreadFunc func, ThreadPool* pool);

private:
    pthread_t tid_;
    ThreadFunc func_;
};

class ThreadPool {
public:
    ThreadPool(int numThreads);

    ~ThreadPool();

    void run();

    // 因为类会自动给非静态成员函数插入this指针参数，
    // 对于线程函数来说，会造成参数不匹配，
    // 所以设置成静态成员函数，以禁止类自动插入this指针参数
    static void* threadFunc(void* arg);

    Queue<WorkType>* getWorkQueue();

    std::vector<Thread*>* getThreads(); 

private:
    std::vector<Thread*> threads_;
    Queue<WorkType> workQueue_;
    int numThreads_;
};

} // namespace chat

#endif // _CHATROOM_SRC_THREADPOOL_H_
