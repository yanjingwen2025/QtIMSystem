#include "network/filetransfermanager.h"
#include "network/tcpclient.h"
#include "network/filesender.h"
#include "network/filereceiver.h"

#include <QDebug>

FileTransferManager::FileTransferManager(TcpClient *client,
                                         QObject *parent)
    : QObject(parent),
    m_client(client)
{
    connect(m_client,
            &TcpClient::fileRequestReceived,
            this,
            &FileTransferManager::onFileRequestReceived);

    connect(m_client,
            &TcpClient::fileChunkReceived,
            this,
            &FileTransferManager::onFileChunkReceived);

    connect(m_client,
            &TcpClient::fileCompleteReceived,
            this,
            &FileTransferManager::onFileCompleteReceived);

    connect(m_client,
            &TcpClient::fileAcceptReceived,
            this,
            &FileTransferManager::onFileAcceptReceived);

    connect(m_client,
            &TcpClient::fileRejectReceived,
            this,
            &FileTransferManager::onFileRejectReceived);

    connect(m_client,
            &TcpClient::fileCompleteAckReceived,
            this,
            &FileTransferManager::onFileCompleteAckReceived);
}

void FileTransferManager::sendFile(const QString &to,
                                   const QString &localFilePath)
{
    auto *sender = new FileSender(m_client,
                                  to,
                                  localFilePath,
                                  this);

    QString fileId = sender->fileId();

    m_senders.insert(fileId, sender);

    FileMeta meta = sender->meta();

    connect(sender, &FileSender::accepted,
            this, [](const QString &) {
                // 状态已经通过 sendStarted / sendCompleted 表达
            });

    connect(sender, &FileSender::rejected,
            this, [this](const QString &id) {
                emit sendRejected(id);
            });

    connect(sender, &FileSender::progressChanged,
            this, [this](const QString &id,
                   qint64 done,
                   qint64 total) {
                emit sendProgress(id, done, total);
            });

    connect(sender, &FileSender::completed,
            this, [this](const QString &id) {
                emit sendCompleted(id);
            });

    connect(sender, &FileSender::failed,
            this, [this](const QString &id,
                   const QString &reason) {
                emit sendFailed(id, reason);
            });

    emit sendStarted(fileId, to, meta);

    m_client->sendFileRequest(to, meta);

    qInfo() << "[FTM] FileRequest sent:"
            << "fileId =" << fileId
            << "fileName =" << meta.fileName
            << "fileSize =" << meta.fileSize;
}

void FileTransferManager::acceptFile(const QString &fileId)
{
    auto *receiver = m_receivers.value(fileId, nullptr);

    if (!receiver) {
        qWarning() << "[FTM] acceptFile: unknown fileId"
                   << fileId;
        return;
    }

    receiver->accept();
}

void FileTransferManager::rejectFile(const QString &fileId)
{
    auto *receiver = m_receivers.value(fileId, nullptr);

    if (!receiver) {
        qWarning() << "[FTM] rejectFile: unknown fileId"
                   << fileId;
        return;
    }

    receiver->reject();
}

void FileTransferManager::onFileRequestReceived(
    const QString &from,
    const FileMeta &meta)
{
    qInfo() << "[FTM] FileRequest received:"
            << "from =" << from
            << "fileId =" << meta.fileId
            << "fileName =" << meta.fileName
            << "fileSize =" << meta.fileSize;

    auto *receiver = new FileReceiver(m_client,
                                      from,
                                      meta,
                                      this);

    m_receivers.insert(meta.fileId, receiver);

    connect(receiver, &FileReceiver::progressChanged,
            this, [this](const QString &id,
                   qint64 done,
                   qint64 total) {
                emit receiveProgress(id, done, total);
            });

    connect(receiver, &FileReceiver::completed,
            this, [this](const QString &id) {
                emit receiveCompleted(id);
            });

    connect(receiver, &FileReceiver::failed,
            this, [this](const QString &id,
                   const QString &reason) {
                emit receiveFailed(id, reason);
            });

    // 不自动接受，交给 UI
    emit fileRequestArrived(from, meta);
}

void FileTransferManager::onFileChunkReceived(
    const QString &from,
    const QString &fileId,
    qint64 offset,
    const QByteArray &data)
{
    Q_UNUSED(from);

    auto *receiver = m_receivers.value(fileId, nullptr);

    if (!receiver) {
        qWarning() << "[FTM] Unknown fileId chunk:"
                   << fileId;
        return;
    }

    receiver->onFileChunk(fileId, offset, data);
}

void FileTransferManager::onFileCompleteReceived(
    const QString &from,
    const QString &fileId,
    const QString &fileMd5)
{
    Q_UNUSED(from);

    auto *receiver = m_receivers.value(fileId, nullptr);

    if (!receiver) {
        qWarning() << "[FTM] Unknown fileId complete:"
                   << fileId;
        return;
    }

    receiver->onFileComplete(fileId, fileMd5);
}

void FileTransferManager::onFileAcceptReceived(
    const QString &from,
    const QString &fileId)
{
    Q_UNUSED(from);

    auto *sender = m_senders.value(fileId, nullptr);

    if (sender) {
        sender->onFileAccept(fileId);
    }
}

void FileTransferManager::onFileRejectReceived(
    const QString &from,
    const QString &fileId)
{
    Q_UNUSED(from);

    auto *sender = m_senders.value(fileId, nullptr);

    if (sender) {
        sender->onFileReject(fileId);
    }
}

void FileTransferManager::onFileCompleteAckReceived(
    const QString &from,
    const QString &fileId)
{
    qInfo() << "[FTM] FileCompleteAck received:"
            << "from =" << from
            << "fileId =" << fileId;

    emit sendAcknowledged(fileId);
}