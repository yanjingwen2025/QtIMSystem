#ifndef MESSAGESTATUS_H
#define MESSAGESTATUS_H

#include <QtGlobal>

// 消息在服务端的持久化状态
enum class MessageStatus : qint32
{
    Pending = 0,     // 本地已创建，等待服务端入库 ACK,消息已入库，但不知道对方有没有收到
    Sent = 1,        // 服务端已入库，但接收方还没确认
    Delivered = 2,   // 接收方已收到,接收方客户端明确回了 DeliveredAck
    Read = 3,        // 接收方已读
    Failed = 4       // 发送失败
};

#endif // MESSAGESTATUS_H