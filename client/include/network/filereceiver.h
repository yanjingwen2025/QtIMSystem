#ifndef FILERECEIVER_H
#define FILERECEIVER_H

#include <QObject>
#include <QString>
#include <QFile>

#include "network/filetransfersession.h"

class TcpClient;

class FileReceiver : public QObject
{
    Q_OBJECT

public:
    explicit FileReceiver(TcpClient *client,
                          const QString &from,
                          const FileMeta &meta,
                          QObject *parent = nullptr);

    QString fileId() const;

    const QString &from() const;

    const FileMeta &meta() const;

signals:
    void accepted(const QString &fileId);
    void rejected(const QString &fileId);

    void progressChanged(const QString &fileId,
                         qint64 sent,
                         qint64 total);
    void completed(const QString &fileId);
    void failed(const QString &fileId,
                const QString &reason);

public slots:
    // UI 决策后调用
    void accept();
    void reject();

    // 收到对方真正开始传文件的数据块时调用（阶段 4 实现）
    void onFileChunk(const QString &fileId,
                     qint64 offset,
                     const QByteArray &data);

    void onFileComplete(const QString &fileId,
                        const QString &md5);

private:
    TcpClient *m_client;
    QString m_from;

    FileTransferSession *m_session = nullptr;

    QFile m_file;
    QString m_savePath;
    QString m_partPath;

    bool openReceiveFile();
    void finalizeReceiveFile();
};

#endif // FILERECEIVER_H