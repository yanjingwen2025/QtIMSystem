#ifndef FILETRANSFERMANAGER_H
#define FILETRANSFERMANAGER_H

#include <QObject>
#include <QHash>
#include <QString>

#include "common/filemeta.h"

class TcpClient;
class FileSender;
class FileReceiver;

class FileTransferManager : public QObject
{
    Q_OBJECT

public:
    explicit FileTransferManager(TcpClient *client,
                                 QObject *parent = nullptr);

    // UI 调用入口
    void sendFile(const QString &to,
                  const QString &localFilePath);

    void acceptFile(const QString &fileId);
    void rejectFile(const QString &fileId);

signals:
    // UI 友好信号
    void fileRequestArrived(const QString &from,
                            const FileMeta &meta);

    void sendStarted(const QString &fileId,
                     const QString &to,
                     const FileMeta &meta);

    void sendProgress(const QString &fileId,
                      qint64 done,
                      qint64 total);

    void sendCompleted(const QString &fileId);

    void sendFailed(const QString &fileId,
                    const QString &reason);

    void sendRejected(const QString &fileId);

    void receiveProgress(const QString &fileId,
                         qint64 done,
                         qint64 total);

    void receiveCompleted(const QString &fileId);

    void receiveFailed(const QString &fileId,
                       const QString &reason);

    void sendAcknowledged(const QString &fileId);

private slots:
    void onFileRequestReceived(const QString &from,
                               const FileMeta &meta);

    void onFileChunkReceived(const QString &from,
                             const QString &fileId,
                             qint64 offset,
                             const QByteArray &data);

    void onFileCompleteReceived(const QString &from,
                                const QString &fileId,
                                const QString &fileMd5);

    void onFileAcceptReceived(const QString &from,
                              const QString &fileId);

    void onFileRejectReceived(const QString &from,
                              const QString &fileId);

    void onFileCompleteAckReceived(const QString &from,
                                   const QString &fileId);

private:
    TcpClient *m_client;

    QHash<QString, FileSender *>   m_senders;
    QHash<QString, FileReceiver *> m_receivers;
};

#endif // FILETRANSFERMANAGER_H