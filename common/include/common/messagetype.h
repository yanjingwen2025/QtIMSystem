#ifndef MESSAGETYPE_H
#define MESSAGETYPE_H

#include <QtGlobal>

enum class MessageType : quint16
{
    Unknown = 0,

    // 连接相关
    Heartbeat = 1,
    HeartbeatAck = 2,

    // 用户认证
    RegisterRequest = 100,
    RegisterResponse = 101,

    LoginRequest = 110,
    LoginResponse = 111,

    // 私聊
    PrivateMessage = 200,
    PrivateMessageAck = 201,

    // 群聊
    GroupMessage = 300,
    GroupMessageAck = 301,

    //好友列表
    FriendListRequest = 120,
    FriendListResponse = 121,

    // 文件
    FileRequest = 400,
    FileChunk = 401,
    FileAck = 402
};

#endif // MESSAGETYPE_H