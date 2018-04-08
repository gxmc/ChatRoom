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
    Buffer() { mutex_ = PTHREAD_MUTEX_INITIALIZER; }

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

    void append(int fd, std::vector<char> buf) {
        pthread_mutex_lock(&mutex_);
        auto iter = buffer_.find(fd);
        if (iter != buffer_.end()) {
            for (int i = 0; i < buf.size(); i++)
                iter->second.push_back(buf[i]);
        }
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
            for (i = 0; i < len; i++) 
                buf.push_back(iter->second[i]);
            std::vector<char> tmp(iter->second.begin() + len, iter->second.end());
            std::swap(tmp, iter->second);
        }
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
