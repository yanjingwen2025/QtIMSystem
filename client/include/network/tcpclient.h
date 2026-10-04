#ifndef TCPCLIENT_H
#define TCPCLIENT_H

#include <QObject>
#include <QTcpSocket>
#include <QTimer>

class TcpClient : public QObject
{
    Q_OBJECT

public:
    explicit TcpClient(QObject *parent = nullptr);

    void connectToServer(const QString &host, quint16 port);

    void sendMessage(const QByteArray &data);

    void login(const QString &username,
               const QString &password);

    void registerUser(const QString &username,
                      const QString &password);

    void sendPrivateMessage(const QString &to,
                            const QString &content);

    void requestFriendList();

signals:
    void loginResult(bool success, const QString &message);
    void registerResult(bool success, const QString &message);
    void connectionStatusChanged(bool connected);
    void privateMessageReceived(const QString &from,
                                const QString &content);
    void friendListReceived(const QStringList &friends);
    void privateMessageAck(const QString &clientMsgId,
                           qint64 messageId);

private slots:
    void onConnected();
    void onDisconnected();
    void onErrorOccurred(QAbstractSocket::SocketError error);
    void onReadyRead();
    void sendHeartbeat();
    void onHeartbeatAck();

private:
    QTcpSocket *m_socket;
    QByteArray m_receiveBuffer;
    QTimer *m_heartbeatTimer;
    qint64 m_lastHeartbeatAckMs;
};

#endif // TCPCLIENT_H