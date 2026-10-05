#ifndef FILEMETA_H
#define FILEMETA_H

#include <QString>
#include <QByteArray>
#include <QtGlobal>
#include <QMetaType>

// 文件传输在协议里交换的核心信息
struct FileMeta
{
    QString fileId;        // 客户端生成的唯一标识（用于匹配同一个文件传输会话）
    QString fileName;      // 显示名（原始文件名）
    qint64  fileSize = 0;  // 字节数
    QString fileMd5;       // 完整文件的 MD5（用于最终校验）
};

// 单个数据块
struct FileChunkMeta
{
    QString fileId;
    qint64  offset = 0;    // 这个块在文件中的起始字节
    QByteArray data;       // 实际数据
};

Q_DECLARE_METATYPE(FileMeta) //把自定义的结构体注册进QT的元对象系统，跨线程传递自定义结构体就得用这个宏，用connect就需要让原系统认识他嘛


#endif // FILEMETA_H