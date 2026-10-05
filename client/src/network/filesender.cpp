#include "network/filesender.h"
#include "network/tcpclient.h"
#include "network/filereaderworker.h"

#include <QFileInfo>
#include <QUuid>
#include <QCryptographicHash>
#include <QDebug>

FileSender::FileSender(TcpClient *client,
                       const QString &to,
                       const QString &localFilePath,
                       QObject *parent)
    : QObject(parent),
    m_client(client),
    m_to(to),
    m_localFilePath(localFilePath)
{
    QFileInfo info(localFilePath);

    FileMeta meta;
    meta.fileId   = QUuid::createUuid().toString(QUuid::WithoutBraces);
    meta.fileName = info.fileName();
    meta.fileSize = info.size();
    meta.fileMd5  = "";

    m_session = new FileTransferSession(
        FileTransferSession::Role::Sender,
        meta,
        this
        );

    m_session->setState(FileTransferSession::State::Idle);

    // ==================== 工作线程 ====================
    m_readerThread = new QThread(this);
    m_reader = new FileReaderWorker();      // ← 没有 parent

    m_reader->moveToThread(m_readerThread);

    connect(m_readerThread, &QThread::finished,
            m_reader, &QObject::deleteLater);

    connect(m_reader, &FileReaderWorker::chunkReady,
            this, &FileSender::onChunkReady);

    connect(m_reader, &FileReaderWorker::finished,
            this, &FileSender::onReaderFinished);

    connect(m_reader, &FileReaderWorker::failed,
            this, &FileSender::onReaderFailed);

    m_readerThread->start();
}

FileSender::~FileSender()
{
    if (m_readerThread) {
        m_readerThread->quit();
        m_readerThread->wait();
    }
}

QString FileSender::fileId() const
{
    return m_session ? m_session->meta().fileId : QString();
}

const QString &FileSender::to() const
{
    return m_to;
}

const QString &FileSender::localFilePath() const
{
    return m_localFilePath;
}

FileMeta FileSender::meta() const
{
    return m_session ? m_session->meta() : FileMeta();
}

void FileSender::startTransfer()
{
    if (m_session->state() == FileTransferSession::State::Transferring) {
        return;
    }

    m_session->setState(FileTransferSession::State::Transferring);

    qInfo() << "[FileSender] startTransfer:"
            << "fileId =" << this->fileId()
            << "fileSize =" << m_session->meta().fileSize;

    QMetaObject::invokeMethod(
        m_reader,
        "start",
        Qt::QueuedConnection,
        Q_ARG(QString, m_localFilePath),
        Q_ARG(qint64, 0),
        Q_ARG(qint64, m_chunkSize)
        );
}

void FileSender::onChunkReady(qint64 offset,
                              const QByteArray &data)
{
    if (data.isEmpty()) {
        return;
    }

    m_client->sendFileChunk(m_to,
                            this->fileId(),
                            offset,
                            data);

    m_session->addSentBytes(data.size());

    emit progressChanged(this->fileId(),
                         m_session->sentBytes(),
                         m_session->meta().fileSize);

    // 通知 worker 读下一块
    QMetaObject::invokeMethod(
        m_reader,
        "readNextChunk",
        Qt::QueuedConnection
        );
}

void FileSender::onReaderFinished(const QByteArray &fileMd5)
{
    qInfo() << "[FileSender] reader finished,"
            << "md5 =" << fileMd5.toHex();

    m_client->sendFileComplete(
        m_to,
        this->fileId(),
        QString::fromLatin1(fileMd5.toHex())
        );
}

void FileSender::onReaderFailed(const QString &reason)
{
    qWarning() << "[FileSender] reader failed:" << reason;

    m_session->setState(FileTransferSession::State::Failed);

    emit failed(this->fileId(), reason);
}

void FileSender::onFileAccept(const QString &fileId)
{
    if (fileId != this->fileId()) {
        return;
    }

    emit accepted(fileId);

    startTransfer();
}

void FileSender::onFileReject(const QString &fileId)
{
    if (fileId != this->fileId()) {
        return;
    }

    m_session->setState(FileTransferSession::State::Rejected);

    emit rejected(fileId);
}
/*
QByteArray FileSender::computeFileMd5()
{
    QFile file(m_localFilePath);

    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Cannot open file for MD5:"
                   << m_localFilePath;
        return {};
    }

    QCryptographicHash hash(QCryptographicHash::Md5);

    const qint64 bufSize = 64 * 1024;

    while (!file.atEnd()) {
        QByteArray buf = file.read(bufSize);
        hash.addData(buf);
    }

    file.close();

    return hash.result();
}
*/