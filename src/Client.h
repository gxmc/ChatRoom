/* 为实现客户端提供的API接口
 */

#ifndef _CHATROOM_SRC_CLIENT_H_
#define _CHATROOM_SRC_CLIENT_H_

#include <string>
#include <vector>

namespace chat {

class Client {
public:
    bool connectServer(std::string serverIp = "127.0.0.1", int serverPort = 5000);

public:
    int signUp(std::string name, std::string password);
    int signIn(std::string name, std::string password);

    std::vector<std::string> lsUsers();
    std::vector<std::string> lsRooms();

    void singleChat(std::string dstName, std::string content);
    void groupChat(std::string dstGroupName, std::string content);

    int mkRoom(std::string roomName);
    int cdRoom(std::string roomName);
    int qtRoom(std::string roomName);

    void exit();

    std::vector<std::string>& getMessage();

private:
    static void* threadFunc(void* arg);
    std::vector<std::string> ls(std::string command);
    void chat(std::string chatType, std::string dstName, std::string content);
    int doRoom(std::string command, std::string roomName);

private:
    std::string serverIp_;
    int serverPort_;
    int socketFd_;

    std::string name_;
    std::string password_;

    std::vector<std::string> message_;
};

} // namespace chat

#endif // _CHATROOM_SRC_CLIENT_H_
