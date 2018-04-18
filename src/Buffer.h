#ifndef __BUFFER_H__
#define __BUFFER_H__

#include <vector>
#include <map>
#include <pthread.h>
#include <algorithm>

#include "Common.h"

namespace chat {

class Buffer {
public:
    Buffer() { pthread_mutex_init(&mutex_, NULL); }
    ~Buffer() { pthread_mutex_destroy(&mutex_); }

    bool exist(int fd) {
        bool ret;

        pthread_mutex_lock(&mutex_);
        auto iter = buffer_.find(fd);
        if (iter != buffer_.end()) 
            ret = true;
        else 
            ret = false;
        pthread_mutex_unlock(&mutex_);

        return ret;
    }

    bool empty(int fd) {
        bool ret;

        pthread_mutex_lock(&mutex_);
        auto iter = buffer_.find(fd);
        if (iter != buffer_.end() && !iter->second.empty())
            ret = false;
        else
            ret = true;
        pthread_mutex_unlock(&mutex_);

        return ret;
    }

    void add(int fd, std::vector<char> buf) {
        pthread_mutex_lock(&mutex_);
        auto iter = buffer_.find(fd);
        if (iter == buffer_.end())
            buffer_[fd] = buf; 
        pthread_mutex_unlock(&mutex_);
    }

    void del(int fd) {
        pthread_mutex_lock(&mutex_);
        auto iter = buffer_.find(fd);
        if (iter != buffer_.end())
            buffer_.erase(iter);
        pthread_mutex_unlock(&mutex_);
    }

    void append(int fd, std::vector<char> buf) {
        pthread_mutex_lock(&mutex_);
        auto iter = buffer_.find(fd);
        if (iter != buffer_.end())
            iter->second.insert(iter->second.end(), buf.begin(), buf.end());
        pthread_mutex_unlock(&mutex_);
    }

    bool retrive(int fd, std::vector<char>& buf, size_t len) {
        bool ret; 
        int i;

        pthread_mutex_lock(&mutex_);
        auto iter = buffer_.find(fd);
        if (iter == buffer_.end() || iter->second.size() < len)
            ret = false;
        else {
            buf.insert(buf.end(), iter->second.begin(), iter->second.begin() + len);
            std::vector<char> tmp(iter->second.begin() + len, iter->second.end());
            std::swap(tmp, iter->second);
        }
        pthread_mutex_unlock(&mutex_);
        return ret;
    }

    size_t size(int fd) {
        size_t bytes = 0;

        pthread_mutex_lock(&mutex_);
        auto iter = buffer_.find(fd);
        if (iter != buffer_.end())
            bytes = iter->second.size();
        pthread_mutex_unlock(&mutex_);
        return bytes;
    }

    void clear(int fd) {
        pthread_mutex_lock(&mutex_);
        auto iter = buffer_.find(fd);
        if (iter != buffer_.end())
            iter->second.clear(); 
        pthread_mutex_unlock(&mutex_);
    }

private:
    std::map<int, std::vector<char>> buffer_; // FD, FD对应的缓冲区
    pthread_mutex_t mutex_;
};

} // namespace chat

#endif //  __BUFFER_H__
