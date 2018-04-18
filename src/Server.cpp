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
    pthread_mutex_init(&msgMutex_, NULL);
}

Server::~Server() {
    pthread_mutex_destroy(&usersMutex_);
    pthread_mutex_destroy(&roomsMutex_);
    pthread_mutex_destroy(&msgMutex_);
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
    int epollFd, numReadyEvents, flags;
    struct epoll_event events[MAXEVENTS];
    struct epoll_event ev;

    epollFd_ = epoll_create1(EPOLL_CLOEXEC);
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = listenFd_;
    flags = fcntl(listenFd_, F_GETFL, 0);
    flags |= O_NONBLOCK;
    fcntl(listenFd_, F_SETFL, flags);
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
    } else if (ev.events & EPOLLOUT) {
        handleWrite(fd);
    } else {
    }
}

/* 处理新连接到来
 *
 * @param listenFd 服务器的监听套接字
 */
void Server::handleAccept(int listenFd) {
    struct sockaddr_in clientAddr;
    socklen_t clientAddrLen;
    int connectedFd, ret, flags;
    struct epoll_event ev;

    while (1) {
        clientAddrLen = sizeof(clientAddr);
        connectedFd = accept(listenFd, (struct sockaddr*)&clientAddr, 
                             &clientAddrLen);
        if (connectedFd == -1) {
            if (errno == EINTR) 
                continue;
            else
                return ;
        }
        break;
    }

    // 非阻塞
    flags = fcntl(connectedFd, F_GETFL, 0);
    flags |= O_NONBLOCK;
    fcntl(connectedFd, F_SETFL, flags);

    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = connectedFd;
    ret = epoll_ctl(epollFd_, EPOLL_CTL_ADD, connectedFd, &ev);
}

/* 处理已连接套接字可读
 *
 * @param fd 活跃的套接字
 */
void Server::handleRead(int fd) {
    int flags, i, ret;
    Message msg;
    bool loop;
    char buf[2048];
    ssize_t bytesRead, bytesTotal;
    size_t bufSize;

    if (!recvBuffer_.exist(fd))
        recvBuffer_.add(fd, std::vector<char>());

    loop = true;

    while (loop) {
        bufSize = recvBuffer_.size(fd);

        if (bufSize != 0) // 该FD的接收缓冲区存有上次未完整接收的数据
            bytesTotal = sizeof(Message) - bufSize;
        else
            bytesTotal = sizeof(Message);

        bytesRead = recv(fd, (void*)buf, bytesTotal, 0); 

        if (bytesRead == bytesTotal) {
            std::vector<std::string> require;

            if (bytesRead == sizeof(Message)) { // 读到一个完整的消息
                strncpy(msg.command, buf, 100); 
                strncpy(msg.dst, buf + 100, 100); 
                strncpy(msg.message, buf + 200, 1024); 
            } else { // 上次和这次的数据合并成一个完整的Message
                std::vector<char> tmp;
                bool ret = recvBuffer_.retrive(fd, tmp, bufSize);
                tmp.insert(tmp.end(), buf, buf + bytesRead);
                for (int i = 0; i < 100; i++)
                    msg.command[i] = tmp[i];
                for (int i = 0; i < 100; i++)
                    msg.dst[i] = tmp[100 + i];
                for (int i = 0; i < 1024; i++)
                    msg.message[i] = tmp[200 + i];
            }

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
            else if (require[0] == "getmsg")
                getMsg(fd);
            else {
            }
        } else {
            if (bytesRead == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) // 处理该FD直到EAGAIN
                    loop = false;
                if (errno == EINTR) {
                    releaseClient(fd);    
                    loop = false;
                }
            } else if (bytesRead == 0) { // 客户端端开
                releaseClient(fd);
                loop = false;
            } else { // bytesRead < bytesTotal
                std::vector<char> tmp;
                tmp.insert(tmp.end(), buf, buf + bytesRead);
                recvBuffer_.append(fd, tmp);
            }
        }
    }
}

void Server::releaseClient(int fd) {
    int ret;
    handleClientClose(fd);
    ret = epoll_ctl(epollFd_, EPOLL_CTL_DEL, fd, NULL);
    recvBuffer_.del(fd);
    sendBuffer_.del(fd);
}

void Server::handleWrite(int fd) {
    size_t bufSize;
    bool flag;
    ssize_t bytesSend;
    int ret;

    if (sendBuffer_.empty(fd))
        return ;

    flag = true;

    while (flag) {
        std::vector<char> tmp;
        char buf[10000];

        bufSize = sendBuffer_.size(fd);

        sendBuffer_.retrive(fd, tmp, bufSize);
        for (int i = 0; i < tmp.size(); i++)
            buf[i] = tmp[i];

        bytesSend = ::send(fd, buf, bufSize, 0);

        if (bytesSend == bufSize) {
            struct epoll_event ev;
            ev.data.fd = fd;
            ev.events = EPOLLIN | EPOLLET;
            ret = epoll_ctl(epollFd_, EPOLL_CTL_MOD, fd, &ev);
        } else if (0 < bytesSend && bytesSend < bufSize) {
            tmp.clear();
            tmp.insert(tmp.end(), buf + bytesSend, buf + bufSize);
            sendBuffer_.append(fd, tmp);
        } else if (bytesSend == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) 
                flag = false; 
            if (errno == EINTR) {
                releaseClient(fd);
                flag = false;
            }
        } else {
        }
    }
}

void Server::Send(int fd, void* buf, size_t len) {
    bool flag;
    ssize_t bytesSend;
    int ret;

    flag = true;
    char* buffer = (char*)buf;
    ssize_t hasSend = 0;

    while (flag) {
        if (!sendBuffer_.empty(fd))
            handleWrite(fd); // 先将发送缓冲区数据发送到客户端

        if (sendBuffer_.empty(fd)) {
            bytesSend = ::send(fd, (void*)((char*)buf + hasSend), len - hasSend, 0); 
            hasSend += bytesSend;

            if (0 < bytesSend && bytesSend < len - hasSend) { // 将未发送完的数据放入发送缓冲区
                std::vector<char> tmp;
                tmp.insert(tmp.end(), buffer + hasSend, buffer + len - bytesSend);
                sendBuffer_.append(fd, tmp);
                hasSend = len;

            } else if (bytesSend == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    struct epoll_event ev; 
                    ev.data.fd = fd;
                    ev.events = EPOLLOUT | EPOLLET;
                    ret = epoll_ctl(epollFd_, EPOLL_CTL_MOD, fd, &ev);
                    break;
                }
                else {
                    releaseClient(fd); 
                    break;
                }
            } else if (bytesSend == 0)
                break;
            else {
            }
        } else { // 追加到发送缓冲区
            std::vector<char> tmp;
            tmp.insert(tmp.end(), buffer, buffer + len);
            sendBuffer_.append(fd, tmp);
            break;
        }
    }
}

/* 客户端关闭处理
 *
 * @param fd 客户端套接字
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
 * @param fd 客户端套接字
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

    Send(fd, (void*)&ret, sizeof(ret));
}

/* 客户端登录
 *
 * @param fd 客户端套接字
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

    Send(fd, (void*)&ret, sizeof(ret));
}

/* 列出当前服务器上所有在线用户
 *
 * @param fd 客户端套接字
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

    Send(fd, (void*)&count, sizeof(count));
    Send(fd, (void*)buf, count * sizeof(Name));
}

/* 单聊
 *
 * @param fd 客户端套接字
 * @param usrName 目的客户端用户名
 * @param content 聊天内容
 */
void Server::singleChat(int fd, std::string usrName, std::string content) {
    int index = -1;
    std::string srcName;

    pthread_mutex_lock(&usersMutex_);
    for (int i = 0; i < users_.size(); i++) {
        if (fd == users_[i].fd)
            srcName = users_[i].name;

        if (users_[i].name == usrName && users_[i].online) {
            index = i;
        }
    }
    pthread_mutex_unlock(&usersMutex_);

    if (index != -1) {
        Message msg;
        strcpy(msg.command, "sgchat");
        strcpy(msg.dst, srcName.c_str());
        strcpy(msg.message, content.c_str());

        pthread_mutex_lock(&msgMutex_);
        auto iter = clientsMsg_.find(users_[index].fd);
        clientsMsg_[users_[index].fd] = msg;
        pthread_mutex_unlock(&msgMutex_);
    }

}

/* 群聊
 *
 * @param fd 客户端套接字
 * @param grpName 群名字
 * @param content 聊天内容
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


    pthread_mutex_lock(&msgMutex_);
    for (int i = 0; i < fds.size(); i++) {
        Message msg; 
        strcpy(msg.command, "gpchat ");
        strcat(msg.command, srcName.c_str());
        strcpy(msg.dst, grpName.c_str());
        strcpy(msg.message, content.c_str());

        clientsMsg_[fds[i]] = msg;
    }
    pthread_mutex_unlock(&msgMutex_);
}

/* 创建房间
 *
 * @param fd 客户端套接字
 * @param roomName 要创建的房间名
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
    Send(fd, (void*)&ret, sizeof(ret));
}

/* 列出服务器上的所有房间名
 *
 * @param fd 客户端套接字
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

    Send(fd, (void*)&count, sizeof(count)); 
    Send(fd, (void*)buf, count * sizeof(Name));
}

/* 客户端进入房间
 *
 * @param fd 客户端套接字
 * @param roomName 房间名
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

    Send(fd, (void*)&ret, sizeof(ret));
}

/* 客户端退出房间
 *
 * @param fd 客户端套接字
 * @param roomName 房间名
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

    Send(fd, (void*)&ret, sizeof(ret));
}

void Server::getMsg(int fd) {
    pthread_mutex_lock(&msgMutex_);

    auto iter = clientsMsg_.find(fd);
    if (iter == clientsMsg_.end()) {
        Message msg;
        strcpy(msg.message, "none");
        Send(fd, &msg, sizeof(msg));
        pthread_mutex_unlock(&msgMutex_);
        return ;
    }

    Send(fd, &iter->second, sizeof(Message));

    pthread_mutex_unlock(&msgMutex_);
}

} // namespace chat
