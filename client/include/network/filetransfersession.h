#ifndef FILETRANSFERSESSION_H
#define FILETRANSFERSESSION_H

#include <QObject>
#include <QString>
#include <QFile>

#include "common/filemeta.h"

class FileTransferSession : public QObject
{
    Q_OBJECT

public:
    enum class Role
    {
        Sender,
        Receiver
    };

    enum class State
    {
        Idle,           // 刚创建，未开始
        WaitingAccept,  // 已发请求，等对方接受
        Transferring,   // 正在传
        Paused,         // 暂停（用于断点续传）
        Completed,      // 完成
        Failed,         // 出错
        Rejected        // 对方拒绝
    };

    explicit FileTransferSession(Role role,
                                 const FileMeta &meta,
                                 QObject *parent = nullptr);

    Role role() const;
    State state() const;

    const FileMeta &meta() const;

    qint64 receivedBytes() const;
    qint64 sentBytes() const;

    void setState(State state);

    void addSentBytes(qint64 bytes);
    void addReceivedBytes(qint64 bytes);

signals:
    void stateChanged(FileTransferSession::State state);
    void progressChanged(qint64 sent, qint64 total);

protected:
    Role m_role;
    FileMeta m_meta;
    State m_state = State::Idle;

    qint64 m_sentBytes = 0;
    qint64 m_receivedBytes = 0;
};

#endif // FILETRANSFERSESSION_H