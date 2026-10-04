#ifndef CLIENTSESSION_H
#define CLIENTSESSION_H

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QDateTime>
#include "common/packet.h"


class QTcpSocket;

class ClientSession : public QObject
{
    Q_OBJECT

public:
    explicit ClientSession(QTcpSocket *socket,
                           QObject *parent = nullptr);

    QTcpSocket *socket() const;

    QString username() const;
    void setUsername(const QString &username);

    bool isAuthenticated() const;
    void setAuthenticated(bool authenticated);

    QByteArray &receiveBuffer();

    QDateTime lastHeartbeatTime() const;
    void updateHeartbeatTime();


signals:
    void packetReceived(ClientSession *session,
                        const Packet &packet);

private slots:
    void onReadyRead();

private:
    QTcpSocket *m_socket;
    QString m_username;
    bool m_authenticated;
    QByteArray m_receiveBuffer;
    QDateTime m_lastHeartbeatTime;
};

#endif // CLIENTSESSION_H
