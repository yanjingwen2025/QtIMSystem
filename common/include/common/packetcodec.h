#ifndef PACKETCODEC_H
#define PACKETCODEC_H

#include <QByteArray>

#include "common/messagetype.h"
#include "common/packet.h"

class PacketCodec
{
public:
    static constexpr quint32 PACKET_MAGIC = 0x51494D53;
    static constexpr quint16 PROTOCOL_VERSION = 1;
    static constexpr qsizetype HEADER_SIZE = 12;

    // 单个消息体最大 10 MB
    static constexpr quint32 MAX_BODY_SIZE = 10 * 1024 * 1024;

    static QByteArray encode(MessageType type, const QByteArray &body);

    static bool decode(QByteArray &buffer, Packet &packet);
};

#endif // PACKETCODEC_H