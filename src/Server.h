/* 服务器端的实现
 *
 */

#ifndef _CHATROOM_SRC_SERVER_H_
#define _CHATROOM_SRC_SERVER_H_

#include <sys/epoll.h>
#include <string>
#include <map>

#include "ThreadPool.h"

namespace chat {

#define NUM_THREADS 10
#define SERVER_PORT 5000
#define BACKLOG 1000
#define MAXEVENTS 100000

// 存放客户端的信息
typedef struct {
    std::string name;
    std::string password;
    int fd;
    bool online;
}User;

class Server {
public:
    Server();
    ~Server();

    void init();
    void eventLoop();

private:
    void solve();
    void handleAccept(int listenFd);
    void handleRead(int fd);
    void handleClientClose(int fd);

private:
    void clientSignUp(int fd, std::vector<std::string> require);
    void clientSignIn(int fd, std::vector<std::string> require);

    void lsUsers(int fd);
    void singleChat(int fd, std::string usrName, std::string content);

    void mkRoom(int fd, std::string roomName);
    void lsRooms(int fd);
    void cdRoom(int fd, std::string roomName);
    void qtRoom(int fd, std::string roomName);
    void groupChat(int fd, std::string grpName, std::string content);

private:
    ThreadPool threadPool_;
    int listenFd_;
    int epollFd_;
    Queue<struct epoll_event> threadPoolArg_;

private:
    // 规定当需要同时对下面两个互斥锁加锁时，
    // 总是先对usersMutex_加锁，再对roomsMutex_加锁，以防止死锁发生
    std::vector<User> users_;
    pthread_mutex_t usersMutex_;

    std::map<std::string, std::vector<std::string>> rooms_;
    pthread_mutex_t roomsMutex_;
};

} // namespace chat

#endif // _CHATROOM_SRC_SERVER_H_
