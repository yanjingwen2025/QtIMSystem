#ifndef TCPCLIENT_H
#define TCPCLIENT_H

#include <QObject>
#include <QTcpSocket>
#include <QTimer>
#include <QHash>
#include "common/messagestatus.h"
#include "common/filemeta.h"

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

    void sendFileRequest(const QString &to,
                         const FileMeta &meta);

    void sendFileAccept(const QString &to,
                        const QString &fileId);

    void sendFileReject(const QString &to,
                        const QString &fileId);

    void sendFileChunk(const QString &to,
                       const QString &fileId,
                       qint64 offset,
                       const QByteArray &data);

    void sendFileComplete(const QString &to,
                          const QString &fileId,
                          const QString &fileMd5);

    void sendFileCompleteAck(const QString &to,
                             const QString &fileId);

signals:
    void loginResult(bool success, const QString &message);
    void registerResult(bool success, const QString &message);
    void connectionStatusChanged(bool connected);
    void privateMessageReceived(const QString &from,
                                const QString &content);
    void friendListReceived(const QStringList &friends);
    void privateMessageAck(const QString &clientMsgId,
                           qint64 messageId);

    void messageStateChanged(const QString &clientMsgId,
                             MessageStatus state);

    void messageCreated(const QString &clientMsgId,
                        const QString &to,
                        const QString &content);

    void fileRequestReceived(const QString &from,
                             const FileMeta &meta);

    void fileAcceptReceived(const QString &from,
                            const QString &fileId);

    void fileRejectReceived(const QString &from,
                            const QString &fileId);

    void fileChunkReceived(const QString &from,
                           const QString &fileId,
                           qint64 offset,
                           const QByteArray &data);

    void fileCompleteReceived(const QString &from,
                              const QString &fileId,
                              const QString &fileMd5);

    void fileCompleteAckReceived(const QString &from,
                                 const QString &fileId);

private slots:
    void onConnected();
    void onDisconnected();
    void onErrorOccurred(QAbstractSocket::SocketError error);
    void onReadyRead();
    void sendHeartbeat();
    void onHeartbeatAck();
    void onPrivateMessageAck(qint64 messageId,
                             const QString &clientMsgId);

    void onPrivateMessageDelivered(qint64 messageId);



private:
    QTcpSocket *m_socket;
    QByteArray m_receiveBuffer;
    QTimer *m_heartbeatTimer;
    qint64 m_lastHeartbeatAckMs;

    struct LocalMessage
    {
        QString clientMsgId;
        QString to;
        QString content;
        qint64  messageId = -1;
        MessageStatus state = MessageStatus::Pending;
    };

    QHash<QString, LocalMessage> m_pendingMessages;
    // key: clientMsgId
    // 用于状态机追踪

    QHash<qint64, QString> m_messageIdToClientMsgId;
    // key: messageId（服务端返回的）
    // value: clientMsgId
    // 用于收到 DeliveredAck 时反查本地消息

    void updateMessageState(const QString &clientMsgId,
                            MessageStatus newState);
};

//Q_DECLARE_METATYPE(FileMeta) //把自定义的结构体注册进QT的元对象系统，跨线程传递自定义结构体就得用这个宏，用connect就需要让原系统认识他嘛

#endif // TCPCLIENT_H