#include "network/tcpclient.h"

#include <QDebug>

#include "common/packet.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QRandomGenerator>

#include "common/packetcodec.h"

TcpClient::TcpClient(QObject *parent)
    : QObject(parent),
    m_socket(new QTcpSocket(this))
{
    connect(m_socket,
            &QTcpSocket::connected,
            this,
            &TcpClient::onConnected);

    connect(m_socket,
            &QTcpSocket::readyRead,
            this,
            &TcpClient::onReadyRead);

    connect(m_socket,
            &QTcpSocket::disconnected,
            this,
            &TcpClient::onDisconnected);

    connect(m_socket,
            &QTcpSocket::errorOccurred,
            this,
            &TcpClient::onErrorOccurred);

    m_heartbeatTimer = new QTimer(this);
    m_heartbeatTimer->setInterval(20000);   // 每 20 秒发一次

    connect(m_heartbeatTimer,
            &QTimer::timeout,
            this,
            &TcpClient::sendHeartbeat);

    m_lastHeartbeatAckMs =
        QDateTime::currentMSecsSinceEpoch();
}

void TcpClient::connectToServer(const QString &host, quint16 port)
{
    qInfo() << "Connecting to server:" << host << port;

    m_socket->connectToHost(host, port);
}

void TcpClient::sendMessage(const QByteArray &data)
{
    if (m_socket->state() != QAbstractSocket::ConnectedState) {
        qWarning() << "Cannot send message: socket is not connected.";
        return;
    }

    QByteArray packet = PacketCodec::encode(
        MessageType::PrivateMessage,
        data
        );

    m_socket->write(packet);

    qInfo() << "Sent packet:"
            << "type ="
            << static_cast<quint16>(MessageType::PrivateMessage)
            << "body ="
            << data;
}

void TcpClient::onConnected()
{
    qInfo() << "Connected to server successfully.";

    m_lastHeartbeatAckMs =
        QDateTime::currentMSecsSinceEpoch();

    m_heartbeatTimer->start();

    emit connectionStatusChanged(true);

}



void TcpClient::onDisconnected()
{
    qInfo() << "Disconnected from server.";

    m_heartbeatTimer->stop();

    emit connectionStatusChanged(false);
}

void TcpClient::onErrorOccurred(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error);

    qWarning() << "Socket error:" << m_socket->errorString();
}

void TcpClient::onReadyRead()
{
    m_receiveBuffer.append(m_socket->readAll());

    Packet packet;

    while (PacketCodec::decode(m_receiveBuffer, packet)) {

        if (packet.header.type == MessageType::RegisterResponse) {

            QJsonParseError error;

            QJsonDocument document =
                QJsonDocument::fromJson(packet.body, &error);

            if (error.error != QJsonParseError::NoError ||
                !document.isObject()) {

                qWarning() << "Invalid register response JSON.";
                continue;
            }

            QJsonObject json = document.object();

            bool success =
                json.value("success").toBool();

            QString message =
                json.value("message").toString();

            qInfo() << "Register response:"
                    << "success =" << success
                    << "message =" << message;
            emit registerResult(success, message);
        }
        else if (packet.header.type == MessageType::LoginResponse) {

            QJsonParseError error;

            QJsonDocument document =
                QJsonDocument::fromJson(packet.body, &error);

            if (error.error != QJsonParseError::NoError ||
                !document.isObject()) {

                qWarning() << "Invalid login response JSON.";
                continue;
            }

            QJsonObject json = document.object();

            bool success =
                json.value("success").toBool();

            QString message =
                json.value("message").toString();

            qInfo() << "Login response:"
                    << "success =" << success
                    << "message =" << message;
            emit loginResult(success, message);
        }
        else if (packet.header.type == MessageType::PrivateMessage)
        {
            QJsonDocument doc = QJsonDocument::fromJson(packet.body);
            QJsonObject json = doc.object();

            QString from = json["from"].toString();
            QString content = json["content"].toString();
            qint64 messageId = json["messageId"].toVariant().toLongLong();

            qInfo() << "Private message received:"
                    << "from =" << from
                    << "content =" << content
                    << "messageId =" << messageId;

            emit privateMessageReceived(from, content);

            // 自动回 DeliveredAck
            if (messageId > 0) {
                QJsonObject ackJson;
                ackJson["messageId"] = messageId;

                QByteArray ackBody =
                    QJsonDocument(ackJson).toJson(QJsonDocument::Compact);

                QByteArray ackPacket =
                    PacketCodec::encode(
                        MessageType::PrivateMessageDelivered,
                        ackBody
                        );

                m_socket->write(ackPacket);

                qInfo() << "DeliveredAck sent for messageId"
                        << messageId;
            }
        }
        else if (packet.header.type == MessageType::FriendListResponse)
        {
            QJsonDocument doc =
                QJsonDocument::fromJson(packet.body);

            QJsonObject json = doc.object();
            QJsonArray friendArray =
                json["friends"].toArray();

            QStringList friends;

            for (const QJsonValue &value : friendArray) {
                friends.append(value.toString());
            }

            emit friendListReceived(friends);
        }
        else if (packet.header.type == MessageType::PrivateMessageAck)
        {
            QJsonDocument doc = QJsonDocument::fromJson(packet.body);
            QJsonObject json = doc.object();

            QString clientMsgId = json["msgId"].toString();
            qint64 messageId = json["messageId"].toVariant().toLongLong();

            qInfo() << "Message ACK:"
                    << "clientMsgId =" << clientMsgId
                    << "messageId =" << messageId;

            emit privateMessageAck(clientMsgId, messageId);

            onPrivateMessageAck(messageId, clientMsgId);
        }
        else if (packet.header.type == MessageType::HeartbeatAck)
        {
            onHeartbeatAck();
        }
        else if (packet.header.type == MessageType::PrivateMessageDelivered)
        {
            QJsonDocument doc = QJsonDocument::fromJson(packet.body);
            QJsonObject json = doc.object();

            qint64 messageId = json["messageId"].toVariant().toLongLong();

            onPrivateMessageDelivered(messageId);
        }
        else if (packet.header.type == MessageType::FileRequest)
        {
            QJsonDocument doc = QJsonDocument::fromJson(packet.body);
            QJsonObject json = doc.object();

            QString from = json["from"].toString();

            FileMeta meta;
            meta.fileId   = json["fileId"].toString();
            meta.fileName = json["fileName"].toString();
            meta.fileSize = json["fileSize"].toVariant().toLongLong();
            meta.fileMd5  = json["fileMd5"].toString();

            qInfo() << "FileRequest received:"
                    << "from =" << from
                    << "fileId =" << meta.fileId
                    << "fileName =" << meta.fileName;

            emit fileRequestReceived(from, meta);
        }
        else if (packet.header.type == MessageType::FileAccept)
        {
            QJsonDocument doc = QJsonDocument::fromJson(packet.body);
            QJsonObject json = doc.object();

            QString from = json["from"].toString();
            QString fileId = json["fileId"].toString();

            qInfo() << "FileAccept received:"
                    << "from =" << from
                    << "fileId =" << fileId;

            emit fileAcceptReceived(from, fileId);
        }
        else if (packet.header.type == MessageType::FileReject)
        {
            QJsonDocument doc = QJsonDocument::fromJson(packet.body);
            QJsonObject json = doc.object();

            QString from = json["from"].toString();
            QString fileId = json["fileId"].toString();

            qInfo() << "FileReject received:"
                    << "from =" << from
                    << "fileId =" << fileId;

            emit fileRejectReceived(from, fileId);
        }
        else if (packet.header.type == MessageType::FileChunk)
        {
            QJsonDocument doc = QJsonDocument::fromJson(packet.body);
            QJsonObject json = doc.object();

            QString from   = json["from"].toString();
            QString fileId = json["fileId"].toString();
            qint64  offset = json["offset"].toVariant().toLongLong();

            QByteArray data =
                QByteArray::fromBase64(
                    json["data"].toString().toLatin1()
                    );

            emit fileChunkReceived(from, fileId, offset, data);
        }
        else if (packet.header.type == MessageType::FileComplete)
        {
            QJsonDocument doc = QJsonDocument::fromJson(packet.body);
            QJsonObject json = doc.object();

            QString from   = json["from"].toString();
            QString fileId = json["fileId"].toString();
            QString md5    = json["fileMd5"].toString();

            emit fileCompleteReceived(from, fileId, md5);
        }
        else if (packet.header.type == MessageType::FileCompleteAck)
        {
            QJsonDocument doc = QJsonDocument::fromJson(packet.body);
            QJsonObject json = doc.object();

            QString from   = json["from"].toString();
            QString fileId = json["fileId"].toString();

            qInfo() << "FileCompleteAck received:"
                    << "from =" << from
                    << "fileId =" << fileId;

            emit fileCompleteAckReceived(from, fileId);
        }
    }
}

void TcpClient::login(const QString &username,
                      const QString &password)
{
    if (m_socket->state() != QAbstractSocket::ConnectedState) {
        qWarning() << "Cannot login: not connected to server.";
        return;
    }

    QJsonObject json;
    json["username"] = username;
    json["password"] = password;

    QByteArray body =
        QJsonDocument(json).toJson(QJsonDocument::Compact);

    QByteArray packet =
        PacketCodec::encode(
            MessageType::LoginRequest,
            body
            );

    m_socket->write(packet);

    qInfo() << "Login request sent:"
            << "username =" << username;

}


void TcpClient::registerUser(const QString &username,
                             const QString &password)
{
    if (m_socket->state() != QAbstractSocket::ConnectedState) {
        qWarning() << "Cannot register: not connected to server.";
        return;
    }

    QJsonObject json;
    json["username"] = username;
    json["password"] = password;

    QByteArray body =
        QJsonDocument(json).toJson(QJsonDocument::Compact);

    QByteArray packet =
        PacketCodec::encode(
            MessageType::RegisterRequest,
            body
            );

    m_socket->write(packet);

    qInfo() << "Register request sent:"
            << "username =" << username;
}

void TcpClient::sendPrivateMessage(const QString &to,
                                   const QString &content)
{
    if (m_socket->state() != QAbstractSocket::ConnectedState) {
        qWarning() << "Cannot send message: not connected";
        return;
    }

    QString clientMsgId =
        QString("%1-%2")
            .arg(QDateTime::currentMSecsSinceEpoch())
            .arg(QRandomGenerator::global()->generate());

    LocalMessage local;
    local.clientMsgId = clientMsgId;
    local.to = to;
    local.content = content;
    local.state = MessageStatus::Pending;

    m_pendingMessages.insert(clientMsgId, local);

    QJsonObject json;
    json["msgId"] = clientMsgId;
    json["to"] = to;
    json["content"] = content;

    QByteArray body =
        QJsonDocument(json).toJson(QJsonDocument::Compact);

    QByteArray packet =
        PacketCodec::encode(MessageType::PrivateMessage, body);

    m_socket->write(packet);

    qInfo() << "Private message sent:"
            << "msgId =" << clientMsgId
            << "to =" << to
            << "content =" << content;

    emit messageCreated(clientMsgId, to, content);
    emit messageStateChanged(clientMsgId, MessageStatus::Pending);

}


void TcpClient::requestFriendList()
{
    if (m_socket->state() != QAbstractSocket::ConnectedState) {
        qWarning() << "Cannot request friend list: not connected";
        return;
    }

    QByteArray packet =
        PacketCodec::encode(
            MessageType::FriendListRequest,
            QByteArray()
            );

    m_socket->write(packet);
}

void TcpClient::sendHeartbeat()
{
    if (m_socket->state() != QAbstractSocket::ConnectedState) {
        return;
    }

    QByteArray packet =
        PacketCodec::encode(
            MessageType::Heartbeat,
            QByteArray()
            );

    m_socket->write(packet);

    //qDebug() << "Heartbeat sent";
}

void TcpClient::onHeartbeatAck()
{
    m_lastHeartbeatAckMs =
        QDateTime::currentMSecsSinceEpoch();

    //qDebug() << "Heartbeat ACK received";
}

void TcpClient::updateMessageState(const QString &clientMsgId,
                                   MessageStatus newState)
{
    auto it = m_pendingMessages.find(clientMsgId);

    if (it == m_pendingMessages.end()) {
        qWarning() << "updateMessageState: unknown clientMsgId"
                   << clientMsgId;
        return;
    }

    MessageStatus current = it->state;

    // 状态机：只允许单向推进，不允许回退
    bool valid = false;

    switch (current) {
    case MessageStatus::Pending:
        valid = (newState == MessageStatus::Sent ||
                 newState == MessageStatus::Failed);
        break;

    case MessageStatus::Sent:
        valid = (newState == MessageStatus::Delivered ||
                 newState == MessageStatus::Failed);
        break;

    case MessageStatus::Delivered:
        valid = (newState == MessageStatus::Read);
        break;

    case MessageStatus::Read:
    case MessageStatus::Failed:
        valid = false;
        break;
    }

    if (!valid) {
        qWarning() << "Invalid state transition:"
                   << static_cast<int>(current)
                   << "->"
                   << static_cast<int>(newState)
                   << "for" << clientMsgId;
        return;
    }

    it->state = newState;

    qInfo() << "Message state:"
            << clientMsgId
            << static_cast<int>(current)
            << "->"
            << static_cast<int>(newState);

    emit messageStateChanged(clientMsgId, newState);
}

void TcpClient::onPrivateMessageAck(
    qint64 messageId,
    const QString &clientMsgId)
{
    auto it = m_pendingMessages.find(clientMsgId);

    if (it == m_pendingMessages.end()) {
        qWarning() << "ACK for unknown clientMsgId"
                   << clientMsgId;
        return;
    }

    it->messageId = messageId;
    m_messageIdToClientMsgId.insert(messageId, clientMsgId);

    updateMessageState(clientMsgId, MessageStatus::Sent);
}

void TcpClient::onPrivateMessageDelivered(qint64 messageId)
{
    QString clientMsgId =
        m_messageIdToClientMsgId.value(messageId);

    if (clientMsgId.isEmpty()) {
        qWarning() << "DeliveredAck for unknown messageId"
                   << messageId;
        return;
    }

    updateMessageState(clientMsgId, MessageStatus::Delivered);
}

void TcpClient::sendFileRequest(const QString &to,
                                const FileMeta &meta)
{
    if (m_socket->state() != QAbstractSocket::ConnectedState) {
        qWarning() << "Cannot send file request: not connected";
        return;
    }

    QJsonObject json;
    json["to"] = to;
    json["fileId"] = meta.fileId;
    json["fileName"] = meta.fileName;
    json["fileSize"] = meta.fileSize;
    json["fileMd5"] = meta.fileMd5;

    QByteArray body =
        QJsonDocument(json).toJson(QJsonDocument::Compact);

    QByteArray packet =
        PacketCodec::encode(MessageType::FileRequest, body);

    m_socket->write(packet);

    qInfo() << "FileRequest sent:"
            << "to =" << to
            << "fileId =" << meta.fileId
            << "fileName =" << meta.fileName
            << "fileSize =" << meta.fileSize;
}

void TcpClient::sendFileAccept(const QString &to,
                               const QString &fileId)
{
    if (m_socket->state() != QAbstractSocket::ConnectedState) {
        qWarning() << "Cannot send file accept: not connected";
        return;
    }

    QJsonObject json;
    json["to"] = to;
    json["fileId"] = fileId;

    QByteArray body =
        QJsonDocument(json).toJson(QJsonDocument::Compact);

    QByteArray packet =
        PacketCodec::encode(MessageType::FileAccept, body);

    m_socket->write(packet);

    qInfo() << "FileAccept sent:"
            << "to =" << to
            << "fileId =" << fileId;
}

void TcpClient::sendFileReject(const QString &to,
                               const QString &fileId)
{
    if (m_socket->state() != QAbstractSocket::ConnectedState) {
        qWarning() << "Cannot send file reject: not connected";
        return;
    }

    QJsonObject json;
    json["to"] = to;
    json["fileId"] = fileId;

    QByteArray body =
        QJsonDocument(json).toJson(QJsonDocument::Compact);

    QByteArray packet =
        PacketCodec::encode(MessageType::FileReject, body);

    m_socket->write(packet);

    qInfo() << "FileReject sent:"
            << "to =" << to
            << "fileId =" << fileId;
}

void TcpClient::sendFileChunk(const QString &to,
                              const QString &fileId,
                              qint64 offset,
                              const QByteArray &data)
{
    if (m_socket->state() != QAbstractSocket::ConnectedState) {
        return;
    }

    QJsonObject json;
    json["to"]     = to;
    json["fileId"] = fileId;
    json["offset"] = offset;
    json["data"]   = QString::fromLatin1(data.toBase64());

    QByteArray body =
        QJsonDocument(json).toJson(QJsonDocument::Compact);

    QByteArray packet =
        PacketCodec::encode(MessageType::FileChunk, body);

    m_socket->write(packet);
}

void TcpClient::sendFileComplete(const QString &to,
                                 const QString &fileId,
                                 const QString &fileMd5)
{
    if (m_socket->state() != QAbstractSocket::ConnectedState) {
        return;
    }

    QJsonObject json;
    json["to"]     = to;
    json["fileId"] = fileId;
    json["fileMd5"] = fileMd5;

    QByteArray body =
        QJsonDocument(json).toJson(QJsonDocument::Compact);

    QByteArray packet =
        PacketCodec::encode(MessageType::FileComplete, body);

    m_socket->write(packet);

    qInfo() << "FileComplete sent:"
            << "fileId =" << fileId
            << "md5 =" << fileMd5;
}

void TcpClient::sendFileCompleteAck(const QString &to,
                                    const QString &fileId)
{
    if (m_socket->state() != QAbstractSocket::ConnectedState) {
        return;
    }

    QJsonObject json;
    json["to"]     = to;
    json["fileId"] = fileId;

    QByteArray body =
        QJsonDocument(json).toJson(QJsonDocument::Compact);

    QByteArray packet =
        PacketCodec::encode(MessageType::FileCompleteAck, body);

    m_socket->write(packet);
}

