#include "network/filereceiver.h"

#include <QDebug>
#include <QDir>
#include <QCoreApplication>
#include <QCryptographicHash>
#include "network/tcpclient.h"

FileReceiver::FileReceiver(TcpClient *client,
                           const QString &from,
                           const FileMeta &meta,
                           QObject *parent)
    : QObject(parent),
    m_client(client),
    m_from(from)
{
    m_session = new FileTransferSession(
        FileTransferSession::Role::Receiver,
        meta,
        this
        );

    m_session->setState(FileTransferSession::State::WaitingAccept);
}

QString FileReceiver::fileId() const
{
    return m_session ? m_session->meta().fileId : QString();
}

const QString &FileReceiver::from() const
{
    return m_from;
}

const FileMeta &FileReceiver::meta() const
{
    return m_session->meta();
}

void FileReceiver::accept()
{
    m_session->setState(
        FileTransferSession::State::Transferring
        );

    emit accepted(fileId());

    m_client->sendFileAccept(m_from, fileId());
}

void FileReceiver::reject()
{
    m_session->setState(
        FileTransferSession::State::Rejected
        );

    emit rejected(fileId());

    m_client->sendFileReject(m_from, fileId());
}
void FileReceiver::onFileChunk(const QString &fileId,
                               qint64 offset,
                               const QByteArray &data)
{
    if (fileId != this->fileId()) {
        return;
    }

    if (!openReceiveFile()) {
        emit failed(fileId, "无法创建本地文件");
        return;
    }

    // 简单处理：假设数据是顺序到达的
    qint64 written = m_file.write(data);

    if (written != data.size()) {
        qWarning() << "File write incomplete:"
                   << written << "/" << data.size();
        emit failed(fileId, "写入本地文件失败");
        return;
    }

    m_session->addReceivedBytes(written);

    emit progressChanged(this->fileId(),
                         m_session->sentBytes(),
                         m_session->meta().fileSize);
}

void FileReceiver::onFileComplete(const QString &fileId,
                                  const QString &md5)
{
    if (fileId != this->fileId()) {
        return;
    }

    if (m_file.isOpen()) {
        m_file.flush();
        m_file.close();
    }

    // 校验 MD5
    QFile file(m_partPath);

    if (!file.open(QIODevice::ReadOnly)) {
        emit failed(fileId, "无法读取接收到的文件");
        return;
    }

    QCryptographicHash hash(QCryptographicHash::Md5);

    const qint64 bufSize = 64 * 1024;
    while (!file.atEnd()) {
        QByteArray buf = file.read(bufSize);
        hash.addData(buf);
    }

    file.close();

    QString actualMd5 =
        QString::fromLatin1(hash.result().toHex());

    if (actualMd5 != md5) {
        qWarning() << "MD5 mismatch:"
                   << "expected =" << md5
                   << "actual =" << actualMd5;

        m_session->setState(
            FileTransferSession::State::Failed
            );

        emit failed(fileId, "文件校验失败");
        return;
    }

    finalizeReceiveFile();

    m_session->setState(
        FileTransferSession::State::Completed
        );

    emit completed(fileId);

    // 回 FileCompleteAck
    m_client->sendFileCompleteAck(m_from, fileId);
}

bool FileReceiver::openReceiveFile()
{
    if (m_file.isOpen()) {
        return true;
    }

    QString saveDir =
        QCoreApplication::applicationDirPath()
        + "/received_files";

    QDir dir;
    if (!dir.exists(saveDir)) {
        if (!dir.mkpath(saveDir)) {
            qWarning() << "Cannot create save dir:"
                       << saveDir;
            return false;
        }
    }

    m_savePath =
        saveDir + "/" + m_session->meta().fileName;

    m_partPath = m_savePath + ".part";

    // 支持断点续传：如果 .part 已存在，就以追加模式打开
    QFileInfo partInfo(m_partPath);

    if (partInfo.exists()) {
        m_session->setState(
            FileTransferSession::State::Paused
            );
    }

    m_file.setFileName(m_partPath);

    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Append)) {
        qWarning() << "Cannot open part file:"
                   << m_partPath;
        return false;
    }

    qInfo() << "FileReceiver openReceiveFile:"
            << "savePath =" << m_savePath
            << "partPath =" << m_partPath
            << "existingBytes =" << partInfo.size();

    return true;
}

void FileReceiver::finalizeReceiveFile()
{
    if (m_file.isOpen()) {
        m_file.close();
    }

    // 重命名 .part → 正式文件
    if (QFile::exists(m_savePath)) {
        QFile::remove(m_savePath);
    }

    if (!QFile::rename(m_partPath, m_savePath)) {
        qWarning() << "Cannot rename part file to:"
                   << m_savePath;
        return;
    }

    qInfo() << "FileReceiver completed:"
            << m_savePath;
}