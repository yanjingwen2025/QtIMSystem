#ifndef TCPSERVER_H
#define TCPSERVER_H

#include <QObject>
#include <QTcpServer>
#include <QHash>
#include <QString>
#include <QTimer>
#include "common/packet.h"

class DatabaseManager;
class QTcpSocket;
class ClientSession;

class TcpServer : public QObject
{
    Q_OBJECT

public:
    explicit TcpServer(DatabaseManager *database,
                       QObject *parent = nullptr);

    bool start(quint16 port);

private slots:
    void onNewConnection();
    void checkHeartbeatTimeout();
    void onPacketReceived(ClientSession *session,
                          const Packet &packet);

private:
    QTcpServer *m_server;
    DatabaseManager *m_database;
    //QHash<QTcpSocket *, QString> m_authenticatedUsers;//每个用户一个连接，简易会话表
    //QHash<QString, QTcpSocket *> m_onlineUsers;//反向记录每个用户对应的连接
    QHash<QString, ClientSession *> m_onlineSessions;

    QTimer *m_heartbeatCheckTimer;

    void handleRegister(ClientSession *session,
                        const Packet &packet);

    void handleLogin(ClientSession *session,
                     const Packet &packet);

    void handleFriendList(ClientSession *session,
                          const Packet &packet);

    void handlePrivateMessage(ClientSession *session,
                              const Packet &packet);

    void handleHeartbeat(ClientSession *session,
                         const Packet &packet);
};

#endif // TCPSERVER_H

