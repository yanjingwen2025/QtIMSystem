#include "common/packetcodec.h"

#include <QDataStream>

#include <QIODevice>

QByteArray PacketCodec::encode(MessageType type, const QByteArray &body)
{
    QByteArray packet;

    QDataStream stream(&packet, QIODevice::WriteOnly);

    stream.setByteOrder(QDataStream::BigEndian);
    stream.setVersion(QDataStream::Qt_6_0);

    stream << PACKET_MAGIC;
    stream << PROTOCOL_VERSION;
    stream << static_cast<quint16>(type);
    stream << static_cast<quint32>(body.size());

    if (!body.isEmpty()) {
        stream.writeRawData(body.constData(),
                            static_cast<qsizetype>(body.size()));
    }

    return packet;
}

bool PacketCodec::decode(QByteArray &buffer, Packet &packet)
{
    // 连固定包头都没有收完整
    if (buffer.size() < HEADER_SIZE) {
        return false;
    }

    QDataStream stream(buffer);

    stream.setByteOrder(QDataStream::BigEndian);
    stream.setVersion(QDataStream::Qt_6_0);

    quint32 magic = 0;
    quint16 version = 0;
    quint16 type = 0;
    quint32 bodyLength = 0;

    stream >> magic;
    stream >> version;
    stream >> type;
    stream >> bodyLength;

    // 检查协议魔数
    if (magic != PACKET_MAGIC) {
        buffer.clear();
        return false;
    }

    // 检查协议版本
    if (version != PROTOCOL_VERSION) {
        buffer.clear();
        return false;
    }

    const qsizetype packetSize =
        HEADER_SIZE + static_cast<qsizetype>(bodyLength);

    // 包头收到了，但 Body 还没有收完整
    if (buffer.size() < packetSize) {
        return false;
    }

    packet.header.magic = magic;
    packet.header.version = version;
    packet.header.type = static_cast<MessageType>(type);
    packet.header.bodyLength = bodyLength;

    if (bodyLength > MAX_BODY_SIZE) {
        buffer.clear();
        return false;
    }

    packet.body = buffer.mid(HEADER_SIZE, bodyLength);

    // 删除已经成功解析的数据
    buffer.remove(0, packetSize);

    return true;
}