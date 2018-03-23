/* 线程安全的队列
 *
 * 1.用作线程池的工作队列
 * 2.用作epoll_wait返回时，存放每个活跃的描述符对应的struct epoll_event
 */

#ifndef _CHATROOM_SRC_QUEUE_H_
#define _CHATROOM_SRC_QUEUE_H_

#include <queue>
#include <pthread.h>

namespace chat {

template <typename T>
class Queue {
public:
    Queue() {
        pthread_mutex_init(&mutex_, NULL);         
        pthread_cond_init(&notEmpty_, NULL);
    }

    ~Queue() {
        pthread_mutex_destroy(&mutex_); 
        pthread_cond_destroy(&notEmpty_);
    }

    void push(const T& w) {
        pthread_mutex_lock(&mutex_); 

        queue_.push(w);
        if (queue_.size() > 0)
            pthread_cond_broadcast(&notEmpty_);

        pthread_mutex_unlock(&mutex_);
    }

    void get(T& w) {
        pthread_mutex_lock(&mutex_); 
        
        while (queue_.size() == 0) {
            pthread_cond_wait(&notEmpty_, &mutex_);
        }

        w = queue_.front();
        queue_.pop();

        pthread_mutex_unlock(&mutex_);
    }

private:
    pthread_mutex_t mutex_;
    pthread_cond_t notEmpty_;
    std::queue<T> queue_;
};

} // namespace chat

#endif // _CHATROOM_SRC_QUEUE_H_
