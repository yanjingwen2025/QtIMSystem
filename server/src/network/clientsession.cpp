#include "network/clientsession.h"

#include <QTcpSocket>
#include <QDateTime>

#include <QDebug>

#include "common/packetcodec.h"

ClientSession::ClientSession(QTcpSocket *socket,
                             QObject *parent)
    : QObject(parent),
    m_socket(socket),
    m_authenticated(false),
    m_lastHeartbeatTime(QDateTime::currentDateTime())
{
    connect(m_socket,
            &QTcpSocket::readyRead,
            this,
            &ClientSession::onReadyRead);
}

QTcpSocket *ClientSession::socket() const
{
    return m_socket;
}

QString ClientSession::username() const
{
    return m_username;
}

void ClientSession::setUsername(const QString &username)
{
    m_username = username;
}

bool ClientSession::isAuthenticated() const
{
    return m_authenticated;
}

void ClientSession::setAuthenticated(bool authenticated)
{
    m_authenticated = authenticated;
}

QByteArray &ClientSession::receiveBuffer()
{
    return m_receiveBuffer;
}

QDateTime ClientSession::lastHeartbeatTime() const
{
    return m_lastHeartbeatTime;
}

void ClientSession::updateHeartbeatTime()
{
    m_lastHeartbeatTime = QDateTime::currentDateTime();
}


void ClientSession::onReadyRead()
{
    m_receiveBuffer.append(m_socket->readAll());

    Packet packet;

    while (PacketCodec::decode(m_receiveBuffer, packet)) {
        updateHeartbeatTime();

        emit packetReceived(this, packet);
    }
}
