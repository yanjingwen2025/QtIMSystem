#ifndef FILESENDER_H
#define FILESENDER_H

#include <QObject>
#include <QString>
#include <QFile>
#include <QThread>

#include "network/filetransfersession.h"

class TcpClient;
class FileReaderWorker;

class FileSender : public QObject
{
    Q_OBJECT

public:
    explicit FileSender(TcpClient *client,
                        const QString &to,
                        const QString &localFilePath,
                        QObject *parent = nullptr);

    ~FileSender();

    QString fileId() const;

    const QString &to() const;
    const QString &localFilePath() const;

    FileMeta meta() const;

signals:
    void requestSent(const QString &fileId);

    void accepted(const QString &fileId);
    void rejected(const QString &fileId);

    void progressChanged(const QString &fileId,
                         qint64 sent,
                         qint64 total);

    void completed(const QString &fileId);
    void failed(const QString &fileId,
                const QString &reason);

public slots:
    void startTransfer();

    void onFileAccept(const QString &fileId);
    void onFileReject(const QString &fileId);

private slots:
    void onChunkReady(qint64 offset,
                      const QByteArray &data);

    void onReaderFinished(const QByteArray &fileMd5);

    void onReaderFailed(const QString &reason);

private:
    TcpClient *m_client;
    QString m_to;
    QString m_localFilePath;

    FileTransferSession *m_session = nullptr;

    QThread *m_readerThread = nullptr;
    FileReaderWorker *m_reader = nullptr;

    qint64 m_chunkSize = 64 * 1024;

   // QByteArray computeFileMd5();
};

#endif // FILESENDER_H