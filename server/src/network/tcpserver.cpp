#include "network/tcpserver.h"

#include <QDebug>
#include <QTcpSocket>
#include <QSharedPointer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <QDateTime>
#include <utility>
#include <QList>
#include "database/databasemanager.h"

#include "common/packetcodec.h"
#include "common/packet.h"
#include "network/clientsession.h"

TcpServer::TcpServer(DatabaseManager *database,
                     QObject *parent)
    : QObject(parent),
    m_server(new QTcpServer(this)),
    m_database(database)
{
    connect(m_server,
            &QTcpServer::newConnection,
            this,
            &TcpServer::onNewConnection);

    m_heartbeatCheckTimer = new QTimer(this);
    m_heartbeatCheckTimer->setInterval(5000);   // 每 5 秒扫一次

    connect(m_heartbeatCheckTimer,
            &QTimer::timeout,
            this,
            &TcpServer::checkHeartbeatTimeout);

    m_heartbeatCheckTimer->start();
}


bool TcpServer::start(quint16 port)
{
    if (!m_server->listen(QHostAddress::Any, port)) {
        qCritical() << "Server failed to start:"
                    << m_server->errorString();
        return false;
    }

    qInfo() << "QtIMServer listening on port:" << port;
    return true;
}

void TcpServer::onNewConnection()
{
    while (m_server->hasPendingConnections()) {
        QTcpSocket *socket = m_server->nextPendingConnection();

        if (!socket) {
            continue;
        }

        qInfo() << "New client connected:"
                << socket->peerAddress().toString()
                << socket->peerPort();

        ClientSession *session =
            new ClientSession(socket, socket);

        connect(session,
                &ClientSession::packetReceived,
                this,
                &TcpServer::onPacketReceived);

        connect(socket,
                &QTcpSocket::disconnected,
                this,
                [this, socket, session]()
                {
                    QString username = session->username();

                    if (!username.isEmpty()) {
                        if (m_onlineSessions.value(username) == session) {
                            m_onlineSessions.remove(username);
                        }

                        qInfo() << "User offline:" << username;
                    }

                    qInfo() << "Client disconnected:"
                            << socket->peerAddress().toString()
                            << socket->peerPort();

                    socket->deleteLater();
                });
    }
}

void TcpServer::checkHeartbeatTimeout()
{
    const qint64 timeoutMs = 30000;   // 30 秒没心跳判定超时
    QDateTime now = QDateTime::currentDateTime();

    QList<QString> timeoutUsers;

    for (auto it = m_onlineSessions.constBegin();
         it != m_onlineSessions.constEnd();
         ++it)
    {
        ClientSession *session = it.value();

        qint64 elapsed =
            session->lastHeartbeatTime()
                .msecsTo(now);

        if (elapsed > timeoutMs) {
            timeoutUsers.append(it.key());
        }
    }

    for (const QString &username : std::as_const(timeoutUsers)) {
        ClientSession *session =
            m_onlineSessions.value(username, nullptr);

        if (session == nullptr) {
            continue;
        }

        qWarning() << "Heartbeat timeout:"
                   << username
                   << "— closing session";

        m_onlineSessions.remove(username);

        session->socket()->disconnectFromHost();
    }
}




void TcpServer::onPacketReceived(ClientSession *session,
                                 const Packet &packet)
{
    switch (packet.header.type) {

    case MessageType::RegisterRequest:
        handleRegister(session, packet);
        break;

    case MessageType::LoginRequest:
        handleLogin(session, packet);
        break;

    case MessageType::FriendListRequest:
        handleFriendList(session, packet);
        break;

    case MessageType::PrivateMessage:
        handlePrivateMessage(session, packet);
        break;

    case MessageType::Heartbeat:
        handleHeartbeat(session, packet);
        break;

    default:
        qWarning() << "Unknown packet type:"
                   << static_cast<quint16>(packet.header.type);
        break;
    }
}

void TcpServer::handleRegister(ClientSession *session,
                               const Packet &packet)
{
    QJsonParseError error;

    QJsonDocument document =
        QJsonDocument::fromJson(packet.body, &error);

    if (error.error != QJsonParseError::NoError ||
        !document.isObject()) {

        qWarning() << "Invalid register JSON:"
                   << error.errorString();
        return;
    }

    QJsonObject json = document.object();

    QString username = json.value("username").toString();
    QString password = json.value("password").toString();

    if (username.isEmpty() || password.isEmpty()) {
        qWarning() << "Username or password is empty.";
        return;
    }

    RegisterResult result =
        m_database->registerUser(username, password);

    QJsonObject responseJson;

    switch (result) {
    case RegisterResult::Success:
        responseJson["success"] = true;
        responseJson["message"] = "Register success";
        qInfo() << "Register success:" << username;
        break;

    case RegisterResult::UsernameAlreadyExists:
        responseJson["success"] = false;
        responseJson["message"] = "Username already exists";
        qWarning() << "Username already exists:" << username;
        break;

    case RegisterResult::DatabaseError:
        responseJson["success"] = false;
        responseJson["message"] = "Database error";
        qWarning() << "Register database error:" << username;
        break;
    }

    QByteArray responseBody =
        QJsonDocument(responseJson).toJson(QJsonDocument::Compact);

    QByteArray responsePacket =
        PacketCodec::encode(MessageType::RegisterResponse,
                            responseBody);

    session->socket()->write(responsePacket);
}

void TcpServer::handleLogin(ClientSession *session,
                            const Packet &packet)
{
    QJsonParseError error;

    QJsonDocument document =
        QJsonDocument::fromJson(packet.body, &error);

    if (error.error != QJsonParseError::NoError ||
        !document.isObject()) {

        qWarning() << "Invalid login JSON:"
                   << error.errorString();
        return;
    }

    QJsonObject json = document.object();

    QString username = json.value("username").toString();
    QString password = json.value("password").toString();

    QJsonObject responseJson;

    if (username.isEmpty() || password.isEmpty()) {
        responseJson["success"] = false;
        responseJson["message"] = "Username or password is empty";
    } else {
        LoginResult result =
            m_database->loginUser(username, password);

        switch (result) {

        case LoginResult::Success:
        {
            session->setUsername(username);
            session->setAuthenticated(true);
            m_onlineSessions[username] = session;

            qInfo() << "Login success:" << username;

            QJsonObject successJson;
            successJson["success"] = true;
            successJson["message"] = "Login success";

            QByteArray loginBody =
                QJsonDocument(successJson)
                    .toJson(QJsonDocument::Compact);

            QByteArray loginPacket =
                PacketCodec::encode(
                    MessageType::LoginResponse,
                    loginBody
                    );

            session->socket()->write(loginPacket);

            qint64 receiverId = m_database->getUserId(username);

            if (receiverId >= 0) {
                QList<OfflineMessage> offlineMsgs =
                    m_database->takeOfflineMessages(receiverId);

                if (!offlineMsgs.isEmpty()) {
                    qInfo() << "Pushing offline messages to"
                            << username
                            << "count =" << offlineMsgs.size();
                }

                for (const OfflineMessage &msg :
                     std::as_const(offlineMsgs))
                {
                    QJsonObject msgJson;
                    msgJson["from"] = msg.senderName;
                    msgJson["content"] = msg.content;
                    msgJson["messageId"] = msg.messageId;

                    QByteArray msgBody =
                        QJsonDocument(msgJson)
                            .toJson(QJsonDocument::Compact);

                    QByteArray msgPacket =
                        PacketCodec::encode(
                            MessageType::PrivateMessage,
                            msgBody
                            );

                    session->socket()->write(msgPacket);
                }
            }

            return;   // ← 成功分支自己 write，直接 return
        }

        case LoginResult::UserNotFound:
            responseJson["success"] = false;
            responseJson["message"] = "User not found";
            qWarning() << "Login user not found:" << username;
            break;

        case LoginResult::WrongPassword:
            responseJson["success"] = false;
            responseJson["message"] = "Wrong password";
            qWarning() << "Wrong password:" << username;
            break;

        case LoginResult::DatabaseError:
            responseJson["success"] = false;
            responseJson["message"] = "Database error";
            qWarning() << "Login database error:" << username;
            break;
        }
    }

    QByteArray responseBody =
        QJsonDocument(responseJson).toJson(QJsonDocument::Compact);

    QByteArray responsePacket =
        PacketCodec::encode(MessageType::LoginResponse,
                            responseBody);

    session->socket()->write(responsePacket);
}

void TcpServer::handleFriendList(ClientSession *session,
                                 const Packet &packet)
{
    Q_UNUSED(packet);

    if (!session->isAuthenticated()) {
        qWarning() << "Unauthenticated client requested friend list";
        return;
    }

    QString username = session->username();
    QStringList friends = m_database->getFriends(username);

    QJsonArray friendArray;
    for (const QString &name : std::as_const(friends)) {
        friendArray.append(name);
    }

    QJsonObject responseJson;
    responseJson["friends"] = friendArray;

    QByteArray responseBody =
        QJsonDocument(responseJson).toJson(QJsonDocument::Compact);

    QByteArray responsePacket =
        PacketCodec::encode(MessageType::FriendListResponse,
                            responseBody);

    session->socket()->write(responsePacket);

    qInfo() << "Friend list sent to"
            << username
            << friends;
}

void TcpServer::handlePrivateMessage(ClientSession *session,
                                     const Packet &packet)
{
    if (!session->isAuthenticated()) {
        qWarning() << "Unauthenticated client tried to send message";
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(packet.body);
    QJsonObject json = doc.object();

    QString to = json["to"].toString();
    QString content = json["content"].toString();
    QString clientMsgId = json["msgId"].toString();

    QString from = session->username();

    qInfo() << "Private message:"
            << "from =" << from
            << "to =" << to
            << "content =" << content;

    qint64 senderId = m_database->getUserId(from);
    qint64 receiverId = m_database->getUserId(to);

    if (senderId < 0 || receiverId < 0) {
        qWarning() << "Cannot save message: user not found"
                   << from << to;
        return;
    }

    ClientSession *receiverSession =
        m_onlineSessions.value(to, nullptr);

    MessageStatus status = (receiverSession != nullptr)
                               ? MessageStatus::Delivered
                               : MessageStatus::Pending;

    qint64 messageId = m_database->saveMessage(
        senderId, receiverId, content, status);

    if (messageId < 0) {
        qWarning() << "Failed to persist message";
        return;
    }

    qInfo() << "Message persisted:"
            << "messageId =" << messageId
            << "status =" << static_cast<qint32>(status);

    {
        QJsonObject ackJson;
        ackJson["msgId"] = clientMsgId;
        ackJson["messageId"] = messageId;

        QByteArray ackBody =
            QJsonDocument(ackJson).toJson(QJsonDocument::Compact);

        QByteArray ackPacket =
            PacketCodec::encode(MessageType::PrivateMessageAck,
                                ackBody);

        session->socket()->write(ackPacket);
    }

    if (receiverSession == nullptr) {
        qInfo() << "User is offline, message stored:" << to;
        return;
    }

    QTcpSocket *receiverSocket = receiverSession->socket();

    QJsonObject forwardJson;
    forwardJson["from"] = from;
    forwardJson["content"] = content;
    forwardJson["messageId"] = messageId;

    QByteArray forwardBody =
        QJsonDocument(forwardJson).toJson(QJsonDocument::Compact);

    QByteArray forwardPacket =
        PacketCodec::encode(MessageType::PrivateMessage,
                            forwardBody);

    receiverSocket->write(forwardPacket);

    qInfo() << "Message forwarded:"
            << from << "->" << to;
}

void TcpServer::handleHeartbeat(ClientSession *session,
                                const Packet &packet)
{
    Q_UNUSED(packet);

    QJsonObject ackJson;
    ackJson["time"] = QDateTime::currentMSecsSinceEpoch();

    QByteArray ackBody =
        QJsonDocument(ackJson).toJson(QJsonDocument::Compact);

    QByteArray ackPacket =
        PacketCodec::encode(MessageType::HeartbeatAck, ackBody);

    session->socket()->write(ackPacket);
}
