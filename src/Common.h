/* 服务器程序和提供给客户端的API公用的类型及宏
 */

#ifndef _CHATROOM_SRC_COMMON_H_
#define _CHATROOM_SRC_COMMON_H_

namespace chat {

// 用户注册、登录、创建房间等动作调用的服务器函数的返回值
#define SIGN_UP_SUCCESS                 0x00000001
#define SIGN_UP_FAIL                    0x00000002
#define SIGN_IN_SUCCESS                 0x00000004
#define SIGN_IN_ACCOUNT_NOT_EXISTENT    0x00000008
#define SIGN_IN_PASSWORD_ERROR          0x00000010
#define MAKE_ROOM_SUCCESS               0x00000020
#define MAKE_ROOM_FAIL                  0x00000040
#define GETINTO_ROOM_SUCCESS            0x00000080
#define GETINTO_ROOM_FAIL               0x00000100
#define QUIT_ROOM_SUCCESS               0x00000200
#define QUIT_ROOM_FAIL                  0x00000400

// 用户名和房间名的类型
typedef struct {
    char name[100];
} Name;

/* 客户端和服务器通信的协议格式
 *
 * command : 命令类型
 * dst : 消息接收方
 * message : 消息内容
 */
typedef struct {
    char command[100];
    char dst[100];
    char message[1024];
} Message;

} // namespace chat

#endif // _CHATROOM_SRC_COMMON_H_
