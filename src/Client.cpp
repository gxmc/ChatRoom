#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <sstream>
#include <unistd.h>
#include <fcntl.h>

#include "Client.h"
#include "Common.h"

namespace chat {

/* 客户端连接服务器
 * 并且创建一个线程专门用于接收消息
 *
 * @parm serverIp 服务器IP
 * @parm serverIp 服务器端口
 * @return true : 连接成功; false : 连接失败
 */
bool Client::connectServer(std::string serverIp, int serverPort) {
    struct sockaddr_in serverAddr;
    int ret;

    serverIp_ = serverIp;
    serverPort_ = serverPort;

    socketFd_ = socket(AF_INET, SOCK_STREAM, 0);

    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(serverPort_);
    inet_pton(AF_INET, serverIp_.c_str(), &serverAddr.sin_addr);

    ret = connect(socketFd_, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
    if (ret == -1)
        return false;

    pthread_t tid;
    pthread_create(&tid, NULL, threadFunc, (void*)this);

    return true;
}

/* 接收消息线程的线程函数
 */
void* Client::threadFunc(void* arg) {
    int fd, bytes, size, flags;

    Client* client = (Client*)arg;
    fd = client->socketFd_;
    size = sizeof(Message);

    flags = fcntl(fd, F_GETFL, 0);
    flags |= O_NONBLOCK;
    fcntl(fd, F_SETFL, flags);

    while (true) {
        Message msg; 
        bytes = recv(fd, (void*)&msg, sizeof(msg), 0);
        if (bytes >= size) {
            std::string message;

            message += "(";

            // 群聊消息
            if (strcmp(msg.command, "sgchat") != 0) {
                for (int i = 0; i < sizeof(msg.command); i++) {
                    if (msg.command[i] == ' ') {
                        for (int j = i + 1; msg.command[j] != '\0'; j++)
                            message.push_back(msg.command[j]);
                        break;
                    }
                }
                message += ", ";
            }

            // 单聊消息
            message += msg.dst;
            message += ")";
            message += " : ";
            message += msg.message;

            // 将消息存放到客户端缓冲区
            client->message_.push_back(message);
        }
    }
}

/* 客户端注册
 *
 * @parm name 用户名
 * @parm password 密码
 */
int Client::signUp(std::string name, std::string password) {
    Message msg;
    int ret;

    strcpy(msg.command, "signup");
    strcpy(msg.dst, name.c_str());
    strcpy(msg.message, password.c_str());

    send(socketFd_, (void*)&msg, sizeof(msg), 0);
    recv(socketFd_, (void*)&ret, sizeof(ret), 0);

    return ret;
}

/* 客户端登录
 *
 * @parm name 用户名
 * @parm password 密码
 */
int Client::signIn(std::string name, std::string password) {
    Message msg;
    int ret;

    name_ = name;
    password_ = password;

    strcpy(msg.command, "signin");
    strcpy(msg.dst, name_.c_str());
    strcpy(msg.message, password_.c_str());

    send(socketFd_, (void*)&msg, sizeof(msg), 0);
    recv(socketFd_, (void*)&ret, sizeof(ret), 0);

    return ret;
}

/* 列出服务器上在线的用户或房间
 *
 * @parm command : "lsuser"或"lsroom"
 */
std::vector<std::string> Client::ls(std::string command) {
    Message msg;
    std::vector<std::string> ret;
    int bytes, num;
    Name buf[1000];

    strcpy(msg.command, command.c_str());

    send(socketFd_, (void*)&msg, sizeof(msg), 0);
    recv(socketFd_, (void*)&num, sizeof(num), 0);
    if (num == 0)
        return ret;

    bytes = recv(socketFd_, (void*)buf, sizeof(buf), 0);
    if (bytes > 0) {
        int numUsers = bytes / sizeof(Name);

        for (int i = 0; i < numUsers; i++) {
            std::string name = buf[i].name; 
            ret.push_back(name); 
        }
    }

    return ret;
}

// 列出服务器上在线的用户或房间
std::vector<std::string> Client::lsUsers() {
    return ls("lsuser");
}

/* 单聊或群聊
 *
 * @parm chatType 聊天类型
 * @parm dstName 聊天对象
 * @parm content 聊天内容
 */
void Client::chat(std::string chatType, std::string dstName, std::string content) {
    Message msg;
    strcpy(msg.command, chatType.c_str());
    strcpy(msg.dst, dstName.c_str());
    strcpy(msg.message, content.c_str());

    send(socketFd_, (void*)&msg, sizeof(msg), 0);
}

/* 单聊
 *
 * @parm dstName 聊天对象
 * @parm content 聊天内容
 */
void Client::singleChat(std::string dstName, std::string content) {
    chat("sgchat", dstName, content);
}

/* 群聊
 *
 * @parm dstName 聊天对象
 * @parm content 聊天内容
 */
void Client::groupChat(std::string dstName, std::string content) {
    chat("gpchat", dstName, content);
}

// 列出服务器上的所有群
std::vector<std::string> Client::lsRooms() {
    return ls("lsroom");
}

// 群操作函数
int Client::doRoom(std::string command, std::string roomName) {
    Message msg;
    int ret;

    strcpy(msg.command, command.c_str());
    strcpy(msg.dst, roomName.c_str());

    send(socketFd_, (void*)&msg, sizeof(msg), 0);
    recv(socketFd_, (void*)&ret, sizeof(ret), 0);

    return ret;
}

/* 创建群
 *
 * @parm roomName 群名称
 */
int Client::mkRoom(std::string roomName) {
    return doRoom("mkroom", roomName);
}

/* 进入群
 *
 * @parm roomName 群名称
 */
int Client::cdRoom(std::string roomName) {
    return doRoom("cdroom", roomName);
}

/* 退出群
 *
 * @parm roomName 群名称
 */
int Client::qtRoom(std::string roomName) {
    return doRoom("qtroom", roomName);
}

/* 客户端退出
 */
void Client::exit() {
    close(socketFd_);
}

// 获取单聊或群聊发送过来的消息
std::vector<std::string>& Client::getMessage() {
    return message_;
}

} // namespace chat
