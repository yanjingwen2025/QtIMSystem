#ifndef FILEREADERWORKER_H
#define FILEREADERWORKER_H

#include <QObject>
#include <QFile>
#include <QString>
#include <QByteArray>
#include <QCryptographicHash>

class FileReaderWorker : public QObject
{
    Q_OBJECT

public:
    explicit FileReaderWorker(QObject *parent = nullptr);

public slots:
    void start(const QString &localFilePath,
               qint64 startOffset,
               qint64 chunkSize);

    void readNextChunk();

    void stop();

signals:
    void chunkReady(qint64 offset,
                    const QByteArray &data);

    // 读完时带出 MD5（16 字节原始值）
    void finished(const QByteArray &fileMd5);

    void failed(const QString &reason);

private:
    QFile m_file;
    qint64 m_chunkSize = 64 * 1024;
    qint64 m_offset = 0;
    bool m_running = false;

    QCryptographicHash m_hash{QCryptographicHash::Md5};
};

#endif // FILEREADERWORKER_H