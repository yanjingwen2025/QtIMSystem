#ifndef PACKET_H
#define PACKET_H

#include <QtGlobal>

#include "common/messagetype.h"

struct PacketHeader
{
    quint32 magic;
    quint16 version;
    MessageType type;
    quint32 bodyLength;
};

struct Packet
{
    PacketHeader header;
    QByteArray body;
};

#endif // PACKET_H