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
            QJsonDocument doc =
                QJsonDocument::fromJson(packet.body);

            QJsonObject json = doc.object();

            QString from = json["from"].toString();
            QString content = json["content"].toString();

            qInfo() << "Private message received:"
                    << "from =" << from
                    << "content =" << content;

            emit privateMessageReceived(from, content);
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
            QJsonDocument doc =
                QJsonDocument::fromJson(packet.body);

            QJsonObject json = doc.object();

            QString clientMsgId = json["msgId"].toString();
            qint64 messageId = json["messageId"].toVariant().toLongLong();

            qInfo() << "Message ACK:"
                    << "clientMsgId =" << clientMsgId
                    << "messageId =" << messageId;

            emit privateMessageAck(clientMsgId, messageId);
        }
        else if (packet.header.type == MessageType::HeartbeatAck)
        {
            onHeartbeatAck();
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

    qDebug() << "Heartbeat sent";
}

void TcpClient::onHeartbeatAck()
{
    m_lastHeartbeatAckMs =
        QDateTime::currentMSecsSinceEpoch();

    qDebug() << "Heartbeat ACK received";
}