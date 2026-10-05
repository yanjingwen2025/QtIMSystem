#include "network/filetransfersession.h"

FileTransferSession::FileTransferSession(Role role,
                                         const FileMeta &meta,
                                         QObject *parent)
    : QObject(parent),
    m_role(role),
    m_meta(meta)
{
}

FileTransferSession::Role FileTransferSession::role() const
{
    return m_role;
}

FileTransferSession::State FileTransferSession::state() const
{
    return m_state;
}

const FileMeta &FileTransferSession::meta() const
{
    return m_meta;
}

qint64 FileTransferSession::receivedBytes() const
{
    return m_receivedBytes;
}

qint64 FileTransferSession::sentBytes() const
{
    return m_sentBytes;
}

void FileTransferSession::setState(State state)
{
    if (m_state == state) {
        return;
    }

    m_state = state;

    emit stateChanged(state);
}

void FileTransferSession::addSentBytes(qint64 bytes)
{
    m_sentBytes += bytes;

    emit progressChanged(m_sentBytes, m_meta.fileSize);
}

void FileTransferSession::addReceivedBytes(qint64 bytes)
{
    m_receivedBytes += bytes;

    emit progressChanged(m_receivedBytes, m_meta.fileSize);
}