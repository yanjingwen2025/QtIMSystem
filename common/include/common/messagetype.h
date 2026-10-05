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
    PrivateMessageDelivered = 202,

    // 群聊
    GroupMessage = 300,
    GroupMessageAck = 301,

    //好友列表
    FriendListRequest = 120,
    FriendListResponse = 121,

    // 文件
    FileRequest       = 400,   // 发送方 → 请求发文件
    FileAccept        = 401,   // 接收方 → 同意接收
    FileReject        = 402,   // 接收方 → 拒绝接收
    FileChunk         = 403,   // 发送方 → 一块数据
    FileChunkAck      = 404,   // 接收方 → 块已收到
    FileComplete      = 405,   // 发送方 → 全部发完
    FileCompleteAck   = 406,   // 接收方 → 校验通过
    FileResumeQuery   = 407,   // 接收方 → 我已收到 offset X
    FileResumeAnswer  = 408    // 发送方 → 从 X 继续发
};

#endif // MESSAGETYPE_H