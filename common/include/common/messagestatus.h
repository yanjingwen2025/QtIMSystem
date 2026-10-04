#ifndef MESSAGESTATUS_H
#define MESSAGESTATUS_H

#include <QtGlobal>

// 消息在服务端的持久化状态
enum class MessageStatus : qint32
{
    Pending = 0,     // 已入库，未送达
    Delivered = 1,   // 已送达接收方
    Read = 2         // 接收方已读（预留）
};

#endif // MESSAGESTATUS_H