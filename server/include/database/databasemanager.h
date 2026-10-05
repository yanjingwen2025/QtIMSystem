#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QSqlDatabase>
#include <QString>
#include <QStringList>
#include <QDateTime>
#include "common/messagestatus.h"

enum class RegisterResult
{
    Success,
    UsernameAlreadyExists,
    DatabaseError
};

enum class LoginResult
{
    Success,
    UserNotFound,
    WrongPassword,
    DatabaseError
};

struct OfflineMessage
{
    qint64 messageId;
    QString senderName;
    QString content;
    QDateTime createdAt;
};

class DatabaseManager
{
public:
    DatabaseManager();

    bool initialize();

    RegisterResult registerUser(const QString &username,
                                const QString &password);

    LoginResult loginUser(const QString &username,
                          const QString &password);

    qint64 getUserId(const QString &username);

    bool addFriend(const QString &username,
                   const QString &friendUsername);

    QStringList getFriends(const QString &username);

    qint64 saveMessage(qint64 senderId,
                       qint64 receiverId,
                       const QString &content,
                       MessageStatus status);

    bool markMessageDelivered(qint64 messageId);

    bool getMessageInfo(qint64 messageId,
                        qint64 &senderId,
                        qint64 &receiverId,
                        QString &content);

    QString getUsernameById(qint64 userId);

    QList<OfflineMessage> takeOfflineMessages(qint64 receiverId);

private:
    QSqlDatabase m_database;

    bool createTables();
};

#endif // DATABASEMANAGER_H