#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sstream>

#include "Server.h"
#include "Common.h"

namespace chat {

Server::Server() : threadPool_(NUM_THREADS) {
    threadPool_.run();
    pthread_mutex_init(&usersMutex_, NULL);
    pthread_mutex_init(&roomsMutex_, NULL);
}

Server::~Server() {
    pthread_mutex_destroy(&usersMutex_);
    pthread_mutex_destroy(&roomsMutex_);
}

void Server::init() {
    struct sockaddr_in servaddr;

    listenFd_ = socket(AF_INET, SOCK_STREAM, 0);

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(SERVER_PORT);

    bind(listenFd_, (struct sockaddr*)&servaddr, sizeof(servaddr));

    listen(listenFd_, BACKLOG);
}

/* 服务器的事件驱动函数
 *
 * 对epoll_wait返回的每一个事件，往工作队列中添加一个任务
 * 其中使用了另一个队列存放每个事件的信息
 */
void Server::eventLoop() {
    int epollFd, numReadyEvents;
    struct epoll_event events[MAXEVENTS];
    struct epoll_event ev;

    epollFd_ = epoll_create1(EPOLL_CLOEXEC);
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = listenFd_;
    epoll_ctl(epollFd_, EPOLL_CTL_ADD, listenFd_, &ev);

    Queue<WorkType>* workQueue = threadPool_.getWorkQueue();

    while (true) {
        numReadyEvents = epoll_wait(epollFd_, events, MAXEVENTS, -1); 
        for (int i = 0; i < numReadyEvents; i++) {
            threadPoolArg_.push(events[i]); // 往队列中添加事件信息
            workQueue->push([this] {this->solve();}); // 往工作队列中添加任务
        }
    }

    close(epollFd);
}

/* 每个线程的实际执行函数
 *
 * 从队列中取出一个事件信息，根据事件类型调用不同函数
 */
void Server::solve()
{
    struct epoll_event ev;
    threadPoolArg_.get(ev);

    int fd = ev.data.fd;
    if (fd == listenFd_ && (ev.events & EPOLLIN)) {
        handleAccept(fd); 
    } else if (ev.events & EPOLLIN) {
        handleRead(fd);
    } else
        ;
}

/* 处理新连接到来
 *
 * @parm listenFd 服务器的监听套接字
 */
void Server::handleAccept(int listenFd) {
    struct sockaddr_in clientAddr;
    socklen_t clientAddrLen;
    int connectedFd;
    struct epoll_event ev;

    connectedFd = accept(listenFd, (struct sockaddr*)&clientAddrLen, 
                         &clientAddrLen);

    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = connectedFd;
    int ret = epoll_ctl(epollFd_, EPOLL_CTL_ADD, connectedFd, &ev);
}

/* 处理已连接套接字可读
 *
 * @parm fd 活跃的套接字
 */
void Server::handleRead(int fd) {
    int flags;

    flags = fcntl(fd, F_GETFL, 0);
    flags |= O_NONBLOCK;
    fcntl(fd, F_SETFL, flags);

    Message msg;
    int n = recv(fd, (void*)&msg, sizeof(Message), MSG_DONTWAIT);

    if (n > 0) {
        std::vector<std::string> require;

        require.push_back(std::string(msg.command));
        require.push_back(std::string(msg.dst));
        require.push_back(std::string(msg.message));

        if (require[0] == "signup")
            clientSignUp(fd, require);
        else if (require[0] == "signin")
            clientSignIn(fd, require);
        else if (require[0] == "lsuser")
            lsUsers(fd);
        else if (require[0] == "sgchat")
            singleChat(fd, require[1], require[2]);
        else if (require[0] == "gpchat")
            groupChat(fd, require[1], require[2]);
        else if (require[0] == "mkroom")
            mkRoom(fd, require[1]);
        else if (require[0] == "lsroom")
            lsRooms(fd);
        else if (require[0] == "cdroom")
            cdRoom(fd, require[1]);
        else if (require[0] == "qtroom")
            qtRoom(fd, require[1]);
        else
        ;
    }

    if (n == 0) {
        handleClientClose(fd);
    }
}

/* 客户端关闭处理
 *
 * @parm fd 客户端套接字
 */
void Server::handleClientClose(int fd) {
    close(fd);

    pthread_mutex_lock(&usersMutex_);

    for (int i = 0; i < users_.size(); i++) {
        if (users_[i].fd == fd) {
            users_[i].online = false;
            break;
        }
    }

    pthread_mutex_unlock(&usersMutex_);
}

/* 客户端注册
 *
 * @parm fd 客户端套接字
 * @require 客户端发送过来的协议内容
 */
void Server::clientSignUp(int fd, std::vector<std::string> require) {
    std::string name = require[1];
    std::string password = require[2];
    int ret;

    pthread_mutex_lock(&usersMutex_); 

    for (int i = 0; i < users_.size(); i++) {
        if (users_[i].name == name) {
            ret = SIGN_UP_FAIL; 
            break;
        }
    }

    if (ret != SIGN_UP_FAIL) {
        User usr;
        usr.name = name;
        usr.password = password;
        usr.online = false;
        users_.push_back(usr);

        ret = SIGN_UP_SUCCESS;
    }

    pthread_mutex_unlock(&usersMutex_);

    send(fd, (void*)&ret, sizeof(ret), MSG_DONTWAIT);
}

/* 客户端登录
 *
 * @parm fd 客户端套接字
 * @require 客户端发送过来的协议内容
 */
void Server::clientSignIn(int fd, std::vector<std::string> require) {
    std::string name = require[1];
    std::string password = require[2];
    int ret, i;

    pthread_mutex_lock(&usersMutex_); 

    for (i = 0; i < users_.size(); i++) {
        if (users_[i].name == name) {
            if (users_[i].password == password) {
                ret = SIGN_IN_SUCCESS;
                users_[i].fd = fd;
                users_[i].online = true;
            }
            else
                ret = SIGN_IN_PASSWORD_ERROR;
            break;
        }
    }
    if (i == users_.size())
        ret = SIGN_IN_ACCOUNT_NOT_EXISTENT;

    pthread_mutex_unlock(&usersMutex_);

    send(fd, (void*)&ret, sizeof(ret), MSG_DONTWAIT);
}

/* 列出当前服务器上所有在线用户
 *
 * @parm fd 客户端套接字
 */
void Server::lsUsers(int fd) {
    Name buf[1000];
    int count = 0;

    pthread_mutex_lock(&usersMutex_);
    for (int i = 0; i < users_.size(); i++) {
        if (users_[i].online)
            strcpy(buf[count++].name, users_[i].name.c_str());
    }
    pthread_mutex_unlock(&usersMutex_);

    send(fd, (void*)&count, sizeof(count), MSG_DONTWAIT);

    int haveSend = 0;
    int n = send(fd, (void*)buf, count * sizeof(Name), MSG_DONTWAIT);
    haveSend += n;
    while (haveSend < count * sizeof(Name)) {
        n = send(fd, (void*)(buf + haveSend), 
                 count * sizeof(Name) - haveSend, MSG_DONTWAIT);
        haveSend += n;
    }
}

/* 单聊
 *
 * @parm fd 客户端套接字
 * @parm usrName 目的客户端用户名
 * @parm content 聊天内容
 */
void Server::singleChat(int fd, std::string usrName, std::string content) {
    pthread_mutex_lock(&usersMutex_);

    int index = -1;
    std::string srcName;
    for (int i = 0; i < users_.size(); i++) {
        if (fd == users_[i].fd)
            srcName = users_[i].name;

        if (users_[i].name == usrName && users_[i].online) {
            index = i;
        }
    }

    if (index != -1) {
        Message msg;
        strcpy(msg.command, "sgchat");
        strcpy(msg.dst, srcName.c_str());
        strcpy(msg.message, content.c_str());

        send(users_[index].fd, (void*)&msg, sizeof(msg), MSG_DONTWAIT);
    }

    pthread_mutex_unlock(&usersMutex_);
}

/* 群聊
 *
 * @parm fd 客户端套接字
 * @parm grpName 群名字
 * @parm content 聊天内容
 */
void Server::groupChat(int fd, std::string grpName, std::string content) {
    std::vector<int> fds;
    std::string srcName;

    pthread_mutex_lock(&usersMutex_);
    pthread_mutex_lock(&roomsMutex_);

    for (int i = 0; i < users_.size(); i++) {
        if (users_[i].fd == fd) {
            srcName = users_[i].name;
            break;
        }
    }

    std::vector<std::string> members = rooms_[grpName];
    for (int i = 0; i < members.size(); i++) {
        for (int j = 0; j < users_.size(); j++) {
            if (users_[j].name == members[i] && users_[j].online) 
                fds.push_back(users_[j].fd);
        }
    }

    pthread_mutex_unlock(&roomsMutex_);
    pthread_mutex_unlock(&usersMutex_);


    for (int i = 0; i < fds.size(); i++) {
        Message msg; 
        strcpy(msg.command, "gpchat ");
        strcat(msg.command, srcName.c_str());
        strcpy(msg.dst, grpName.c_str());
        strcpy(msg.message, content.c_str());

        send(fds[i], (void*)&msg, sizeof(msg), MSG_DONTWAIT);
    }
}

/* 创建房间
 *
 * @parm fd 客户端套接字
 * @parm roomName 要创建的房间名
 */
void Server::mkRoom(int fd, std::string roomName) {
    pthread_mutex_lock(&roomsMutex_);

    int ret;
    std::map<std::string, std::vector<std::string>>::iterator iter = rooms_.begin();

    for (; iter != rooms_.end(); iter++) {
        if (iter->first == roomName) {
            ret = MAKE_ROOM_FAIL; 
            break;
        }
    }

    if (ret != MAKE_ROOM_FAIL) {
        rooms_[roomName] = std::vector<std::string>();
        ret = MAKE_ROOM_SUCCESS;
    }

    pthread_mutex_unlock(&roomsMutex_);
    send(fd, (void*)&ret, sizeof(ret), MSG_DONTWAIT);
}

/* 列出服务器上的所有房间名
 *
 * @parm fd 客户端套接字
 */
void Server::lsRooms(int fd) {
    Name buf[1000];
    int count = 0;

    pthread_mutex_lock(&roomsMutex_);
    std::map<std::string, std::vector<std::string>>::iterator iter = rooms_.begin();
    for (; iter != rooms_.end(); iter++) {
        strcpy(buf[count++].name, iter->first.c_str());
    }
    pthread_mutex_unlock(&roomsMutex_);

    send(fd, (void*)&count, sizeof(count), MSG_DONTWAIT); 

    int haveSend = 0;
    int n = send(fd, (void*)buf, count * sizeof(Name), MSG_DONTWAIT);
    haveSend += n;
    while (haveSend < count * sizeof(Name)) {
        n = send(fd, (void*)(buf + haveSend), 
                 count * sizeof(Name) - haveSend, MSG_DONTWAIT);
        haveSend += n;
    }
}

/* 客户端进入房间
 *
 * @parm fd 客户端套接字
 * @parm roomName 房间名
 */
void Server::cdRoom(int fd, std::string roomName) {
    int ret;
    std::string userName;

    pthread_mutex_lock(&usersMutex_);

    for (int i = 0; i < users_.size(); i++) {
        if (users_[i].fd == fd) {
            userName = users_[i].name;
            break;
        }
    }

    if (userName.empty())
        ret = GETINTO_ROOM_FAIL;
    else {
        pthread_mutex_lock(&roomsMutex_);
        auto iter = rooms_.find(roomName); 
        if (iter == rooms_.end())
            ret = GETINTO_ROOM_FAIL; 
        else {
            iter->second.push_back(userName);
            ret = GETINTO_ROOM_SUCCESS;
        }
        pthread_mutex_unlock(&roomsMutex_);
    }

    pthread_mutex_unlock(&usersMutex_);

    send(fd, (void*)&ret, sizeof(ret), MSG_DONTWAIT);
}

/* 客户端退出房间
 *
 * @parm fd 客户端套接字
 * @parm roomName 房间名
 */
void Server::qtRoom(int fd, std::string roomName) {
    int ret;
    std::string userName;

    pthread_mutex_lock(&usersMutex_);

    for (int i = 0; i < users_.size(); i++) {
        if (users_[i].fd == fd) {
            userName = users_[i].name;
            break;
        }
    }

    if (userName.empty())
        ret = QUIT_ROOM_FAIL;
    else {
        pthread_mutex_lock(&roomsMutex_);
        auto iter = rooms_.find(roomName); 
        if (iter == rooms_.end()) {
            ret = QUIT_ROOM_FAIL; 
        } else {
            int index = -1;
            std::vector<std::string> members = iter->second;
            for (int i = 0; i < members.size(); i++) {
                if (userName == members[i]) {
                    index = i;
                    break;
                }
            }

            if (index != -1) {
                iter->second.erase(iter->second.begin() + index);
                ret = QUIT_ROOM_SUCCESS;
            } else {
                ret = QUIT_ROOM_FAIL; 
            }
        }
        pthread_mutex_unlock(&roomsMutex_);
    }

    pthread_mutex_unlock(&usersMutex_);

    send(fd, (void*)&ret, sizeof(ret), MSG_DONTWAIT);
}

} // namespace chat
