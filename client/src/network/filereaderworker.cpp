#include "network/filereaderworker.h"

#include <QDebug>

FileReaderWorker::FileReaderWorker(QObject *parent)
    : QObject(parent)
{
}

void FileReaderWorker::start(const QString &localFilePath,
                             qint64 startOffset,
                             qint64 chunkSize)
{
    if (m_running) {
        return;
    }

    m_file.setFileName(localFilePath);

    if (!m_file.open(QIODevice::ReadOnly)) {
        emit failed("Cannot open file: " + localFilePath);
        return;
    }

    m_chunkSize = chunkSize;
    m_offset = startOffset;

    if (!m_file.seek(m_offset)) {
        emit failed("Cannot seek file to offset");
        m_file.close();
        return;
    }

    m_running = true;

    qInfo() << "[FileReaderWorker] start:"
            << localFilePath
            << "offset =" << startOffset
            << "chunkSize =" << chunkSize;

    readNextChunk();
}

void FileReaderWorker::readNextChunk()
{
    if (!m_running) {
        return;
    }

    QByteArray chunk = m_file.read(m_chunkSize);

    if (chunk.isEmpty()) {
        m_running = false;
        m_file.close();

        QByteArray md5 = m_hash.result();

        qInfo() << "[FileReaderWorker] finished,"
                << "total read =" << m_offset
                << "md5 =" << md5.toHex();

        emit finished(md5);
        return;
    }

    // 边读边算 MD5
    m_hash.addData(chunk);

    qint64 currentOffset = m_offset;
    m_offset += chunk.size();

    emit chunkReady(currentOffset, chunk);
}

void FileReaderWorker::stop()
{
    if (!m_running) {
        return;
    }

    m_running = false;

    if (m_file.isOpen()) {
        m_file.close();
    }
}