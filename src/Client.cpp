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
 * @param serverIp 服务器IP
 * @param serverIp 服务器端口
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

    return true;
}

/* 客户端注册
 *
 * @param name 用户名
 * @param password 密码
 */
int Client::signUp(std::string name, std::string password) {
    Message msg;
    int ret, bytes;

    strcpy(msg.command, "signup");
    strcpy(msg.dst, name.c_str());
    strcpy(msg.message, password.c_str());


    bytes = Send(socketFd_, &msg, sizeof(msg));
    if (bytes != sizeof(msg))
        return SIGN_UP_FAIL;

    bytes = Recv(socketFd_, (void*)&ret, sizeof(ret));
    if (bytes != sizeof(ret))
        return SIGN_UP_FAIL;

    return ret;
}

/* 客户端登录
 *
 * @param name 用户名
 * @param password 密码
 */
int Client::signIn(std::string name, std::string password) {
    Message msg;
    int ret, bytes;

    name_ = name;
    password_ = password;

    strcpy(msg.command, "signin");
    strcpy(msg.dst, name_.c_str());
    strcpy(msg.message, password_.c_str());

    bytes = Send(socketFd_, (void*)&msg, sizeof(msg));
    if (bytes != sizeof(msg))
        return !SIGN_IN_SUCCESS;

    bytes = Recv(socketFd_, (void*)&ret, sizeof(ret));
    if (bytes != sizeof(ret))
        return !SIGN_IN_SUCCESS;

    return ret;
}

/* 列出服务器上在线的用户或房间
 *
 * @param command : "lsuser"或"lsroom"
 */
std::vector<std::string> Client::ls(std::string command) {
    Message msg;
    std::vector<std::string> ret;
    int bytes, num;
    Name buf[1000];

    strcpy(msg.command, command.c_str());

    bytes = Send(socketFd_, (void*)&msg, sizeof(msg));
    if (bytes != sizeof(msg))
        return ret;

    bytes = Recv(socketFd_, (void*)&num, sizeof(num));
    if (bytes != sizeof(num) || num == 0)
        return ret;

    bytes = Recv(socketFd_, (void*)buf, num * sizeof(Name));
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
 * @param chatType 聊天类型
 * @param dstName 聊天对象
 * @param content 聊天内容
 */
void Client::chat(std::string chatType, std::string dstName, std::string content) {
    Message msg;
    strcpy(msg.command, chatType.c_str());
    strcpy(msg.dst, dstName.c_str());
    strcpy(msg.message, content.c_str());

    Send(socketFd_, (void*)&msg, sizeof(msg));
}

/* 单聊
 *
 * @param dstName 聊天对象
 * @param content 聊天内容
 */
void Client::singleChat(std::string dstName, std::string content) {
    chat("sgchat", dstName, content);
}

/* 群聊
 *
 * @param dstName 聊天对象
 * @param content 聊天内容
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
    int ret, bytes;

    strcpy(msg.command, command.c_str());
    strcpy(msg.dst, roomName.c_str());

    bytes = Send(socketFd_, (void*)&msg, sizeof(msg));
    if (bytes != sizeof(msg))
        return -1;

    bytes = Recv(socketFd_, (void*)&ret, sizeof(ret));
    if (bytes != sizeof(ret))
        return -1;

    return ret;
}

/* 创建群
 *
 * @param roomName 群名称
 */
int Client::mkRoom(std::string roomName) {
    return doRoom("mkroom", roomName);
}

/* 进入群
 *
 * @param roomName 群名称
 */
int Client::cdRoom(std::string roomName) {
    return doRoom("cdroom", roomName);
}

/* 退出群
 *
 * @param roomName 群名称
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
    Message msg; 
    ssize_t bytes;

    strcpy(msg.command, "getmsg");

    bytes = Send(socketFd_, (void*)&msg, sizeof(msg));
    if (bytes != sizeof(msg))
        return message_;

    bytes = Recv(socketFd_, (void*)&msg, sizeof(msg));

    if (bytes == sizeof(msg) && strcmp(msg.message, "none") != 0) {
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
        message_.push_back(message);
    }

    return message_;
}

ssize_t Client::Recv(int fd, void* buf, size_t len) {
    size_t bytes, hasRead;

    hasRead = 0;

    while (hasRead != len) {
        bytes = recv(fd, (void*)((char*)buf + hasRead), len - hasRead, 0); 
        if (bytes <= 0)
            break;

        hasRead += bytes;
    }
    return hasRead;
}

ssize_t Client::Send(int fd, void* buf, size_t len) {
    size_t bytes, hasWrite;

    hasWrite = 0;

    while (hasWrite != len) {
        bytes = send(fd, (void*)((char*)buf + hasWrite), len - hasWrite, 0); 
        if (bytes <= 0)
            break;

        hasWrite += bytes;
    }
    return hasWrite;
}

} // namespace chat
