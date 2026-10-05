#include "database/databasemanager.h"

#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>

#include <QCryptographicHash>
#include <QRandomGenerator>
#include <utility>

DatabaseManager::DatabaseManager()
{
}

bool DatabaseManager::initialize()
{
    m_database = QSqlDatabase::addDatabase("QSQLITE");
    m_database.setDatabaseName("qtim.db");

    if (!m_database.open()) {
        qCritical() << "Failed to open database:"
                    << m_database.lastError().text();
        return false;
    }

    qInfo() << "Database opened successfully.";

    return createTables();
}

bool DatabaseManager::createTables()
{
    QSqlQuery query(m_database);

    const QString sql =
        "CREATE TABLE IF NOT EXISTS users ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "username TEXT NOT NULL UNIQUE,"
        "password_hash TEXT NOT NULL,"
        "salt TEXT NOT NULL,"
        "created_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ")";

    if (!query.exec(sql)) {
        qCritical() << "Failed to create users table:"
                    << query.lastError().text();
        return false;
    }

    QString createFriendshipsTable =
        "CREATE TABLE IF NOT EXISTS friendships ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "user_id INTEGER NOT NULL,"
        "friend_id INTEGER NOT NULL,"
        "created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "UNIQUE(user_id, friend_id),"
        "FOREIGN KEY(user_id) REFERENCES users(id),"
        "FOREIGN KEY(friend_id) REFERENCES users(id)"
        ")";

    if (!query.exec(createFriendshipsTable)) {
        qCritical() << "Failed to create friendships table:"
                    << query.lastError().text();
        return false;
    }

    QString createMessagesTable =
        "CREATE TABLE IF NOT EXISTS messages ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "sender_id INTEGER NOT NULL,"
        "receiver_id INTEGER NOT NULL,"
        "content TEXT NOT NULL,"
        "status INTEGER NOT NULL DEFAULT 0,"
        "created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "FOREIGN KEY(sender_id) REFERENCES users(id),"
        "FOREIGN KEY(receiver_id) REFERENCES users(id)"
        ")";

    if (!query.exec(createMessagesTable)) {
        qCritical() << "Failed to create messages table:"
                    << query.lastError().text();
        return false;
    }

    qInfo() << "Users table ready.";

    return true;
}

RegisterResult DatabaseManager::registerUser(
    const QString &username,
    const QString &password)
{
    // 先检查用户名是否已经存在
    QSqlQuery checkQuery(m_database);

    checkQuery.prepare(
        "SELECT id FROM users "
        "WHERE username = :username"
        );

    checkQuery.bindValue(":username", username);

    if (!checkQuery.exec()) {
        qWarning() << "Failed to check username:"
                   << checkQuery.lastError().text();

        return RegisterResult::DatabaseError;
    }

    if (checkQuery.next()) {
        return RegisterResult::UsernameAlreadyExists;
    }


    QByteArray salt;

    for (int i = 0; i < 16; ++i) {
        salt.append(
            static_cast<char>(
                QRandomGenerator::global()->bounded(256)
                )
            );
    }

    QByteArray passwordHash =
        QCryptographicHash::hash(
            salt + password.toUtf8(),
            QCryptographicHash::Sha256
            );


    // 用户名不存在，执行注册
    QSqlQuery insertQuery(m_database);

    insertQuery.prepare(
        "INSERT INTO users "
        "(username, password_hash, salt) "
        "VALUES (:username, :password_hash, :salt)"
        );

    insertQuery.bindValue(
        ":username",
        username
        );

    insertQuery.bindValue(
        ":password_hash",
        QString::fromLatin1(passwordHash.toHex())
        );

    insertQuery.bindValue(
        ":salt",
        QString::fromLatin1(salt.toHex())
        );

    if (!insertQuery.exec()) {
        qWarning() << "Failed to register user:"
                   << insertQuery.lastError().text();

        return RegisterResult::DatabaseError;
    }

    qInfo() << "User registered successfully:"
            << username;

    return RegisterResult::Success;
}

LoginResult DatabaseManager::loginUser(
    const QString &username,
    const QString &password)
{
    QSqlQuery query(m_database);

    query.prepare(
        "SELECT password_hash, salt "
        "FROM users "
        "WHERE username = :username"
        );

    query.bindValue(":username", username);

    if (!query.exec()) {
        qWarning() << "Failed to query user:"
                   << query.lastError().text();

        return LoginResult::DatabaseError;
    }

    if (!query.next()) {
        return LoginResult::UserNotFound;
    }

    QString storedHash =
        query.value("password_hash").toString();

    QString storedSalt =
        query.value("salt").toString();

    QByteArray salt =
        QByteArray::fromHex(storedSalt.toLatin1());

    QByteArray inputHash =
        QCryptographicHash::hash(
            salt + password.toUtf8(),
            QCryptographicHash::Sha256
            );

    QString inputHashHex =
        QString::fromLatin1(inputHash.toHex());

    if (inputHashHex != storedHash) {
        return LoginResult::WrongPassword;
    }

    return LoginResult::Success;
}


qint64 DatabaseManager::getUserId(const QString &username)
{
    QSqlQuery query(m_database);

    query.prepare(
        "SELECT id FROM users "
        "WHERE username = :username"
        );

    query.bindValue(":username", username);

    if (!query.exec()) {
        qWarning() << "Failed to get user id:"
                   << query.lastError().text();
        return -1;
    }

    if (!query.next()) {
        return -1;
    }

    return query.value(0).toLongLong();
}

bool DatabaseManager::addFriend(
    const QString &username,
    const QString &friendUsername)
{
    qint64 userId = getUserId(username);
    qint64 friendId = getUserId(friendUsername);

    if (userId < 0 || friendId < 0) {
        qWarning() << "Cannot add friend: user not found";
        return false;
    }

    if (userId == friendId) {
        qWarning() << "Cannot add yourself as friend";
        return false;
    }

    if (!m_database.transaction()) {
        qWarning() << "Failed to start transaction:"
                   << m_database.lastError().text();
        return false;
    }

    QSqlQuery query(m_database);

    query.prepare(
        "INSERT OR IGNORE INTO friendships "
        "(user_id, friend_id) "
        "VALUES (:userId, :friendId)"
        );

    query.bindValue(":userId", userId);
    query.bindValue(":friendId", friendId);

    if (!query.exec()) {
        qWarning() << "Failed to add friendship:"
                   << query.lastError().text();
        m_database.rollback();
        return false;
    }

    query.prepare(
        "INSERT OR IGNORE INTO friendships "
        "(user_id, friend_id) "
        "VALUES (:userId, :friendId)"
        );

    query.bindValue(":userId", friendId);
    query.bindValue(":friendId", userId);

    if (!query.exec()) {
        qWarning() << "Failed to add reverse friendship:"
                   << query.lastError().text();
        m_database.rollback();
        return false;
    }

    if (!m_database.commit()) {
        qWarning() << "Failed to commit friendship:"
                   << m_database.lastError().text();
        m_database.rollback();
        return false;
    }

    return true;
}

QStringList DatabaseManager::getFriends(
    const QString &username)
{
    QStringList friends;

    qint64 userId = getUserId(username);

    if (userId < 0) {
        return friends;
    }

    QSqlQuery query(m_database);

    query.prepare(
        "SELECT u.username "
        "FROM friendships f "
        "JOIN users u ON f.friend_id = u.id "
        "WHERE f.user_id = :userId "
        "ORDER BY u.username"
        );

    query.bindValue(":userId", userId);

    if (!query.exec()) {
        qWarning() << "Failed to get friends:"
                   << query.lastError().text();
        return friends;
    }

    while (query.next()) {
        friends.append(
            query.value(0).toString() //第0列
            );
    }

    return friends;
}


qint64 DatabaseManager::saveMessage(
    qint64 senderId,
    qint64 receiverId,
    const QString &content,
    MessageStatus status)
{
    QSqlQuery query(m_database);

    query.prepare(
        "INSERT INTO messages "
        "(sender_id, receiver_id, content, status) "
        "VALUES (:senderId, :receiverId, :content, :status)"
        );

    query.bindValue(":senderId", senderId);
    query.bindValue(":receiverId", receiverId);
    query.bindValue(":content", content);
    query.bindValue(":status", static_cast<qint32>(status));

    if (!query.exec()) {
        qWarning() << "Failed to save message:"
                   << query.lastError().text();
        return -1;
    }

    return query.lastInsertId().toLongLong();
}

QList<OfflineMessage> DatabaseManager::takeOfflineMessages(
    qint64 receiverId)
{
    QList<OfflineMessage> result;

    if (!m_database.transaction()) {
        qWarning() << "Failed to start transaction:"
                   << m_database.lastError().text();
        return result;
    }

    QSqlQuery query(m_database);

    query.prepare(
        "SELECT m.id, u.username, m.content, m.created_at "
        "FROM messages m "
        "JOIN users u ON m.sender_id = u.id "
        "WHERE m.receiver_id = :receiverId "
        "AND m.status = 0 "
        "ORDER BY m.id ASC"
        );

    query.bindValue(":receiverId", receiverId);

    if (!query.exec()) {
        qWarning() << "Failed to query offline messages:"
                   << query.lastError().text();
        m_database.rollback();
        return result;
    }

    QList<qint64> messageIds;

    while (query.next()) {
        OfflineMessage msg;
        msg.messageId = query.value(0).toLongLong();
        msg.senderName = query.value(1).toString();
        msg.content = query.value(2).toString();
        msg.createdAt = query.value(3).toDateTime();

        result.append(msg);
        messageIds.append(msg.messageId);
    }

    // 标记为已投递
    for (qint64 id : std::as_const(messageIds)) {
        QSqlQuery update(m_database);

        update.prepare(
            "UPDATE messages "
            "SET status = :deliveredStatus "
            "WHERE id = :id"
            );

        update.bindValue(":deliveredStatus",
                         static_cast<qint32>(MessageStatus::Delivered));
        update.bindValue(":id", id);

        if (!update.exec()) {
            qWarning() << "Failed to update message status:"
                       << update.lastError().text();
            m_database.rollback();
            return {};
        }
    }

    if (!m_database.commit()) {
        qWarning() << "Failed to commit offline messages:"
                   << m_database.lastError().text();
        m_database.rollback();
        return {};
    }

    return result;
}


bool DatabaseManager::markMessageDelivered(qint64 messageId)
{
    QSqlQuery query(m_database);

    query.prepare(
        "UPDATE messages "
        "SET status = :status "
        "WHERE id = :id"
        );

    query.bindValue(":status",
                    static_cast<qint32>(MessageStatus::Delivered));
    query.bindValue(":id", messageId);

    if (!query.exec()) {
        qWarning() << "Failed to mark message delivered:"
                   << query.lastError().text();
        return false;
    }

    if (query.numRowsAffected() <= 0) {
        qWarning() << "No message updated for id =" << messageId;
        return false;
    }

    return true;
}

bool DatabaseManager::getMessageInfo(
    qint64 messageId,
    qint64 &senderId,
    qint64 &receiverId,
    QString &content)
{
    QSqlQuery query(m_database);

    query.prepare(
        "SELECT sender_id, receiver_id, content "
        "FROM messages "
        "WHERE id = :id"
        );

    query.bindValue(":id", messageId);

    if (!query.exec()) {
        qWarning() << "Failed to query message info:"
                   << query.lastError().text();
        return false;
    }

    if (!query.next()) {
        return false;
    }

    senderId = query.value(0).toLongLong();
    receiverId = query.value(1).toLongLong();
    content = query.value(2).toString();

    return true;
}

QString DatabaseManager::getUsernameById(qint64 userId)
{
    QSqlQuery query(m_database);

    query.prepare(
        "SELECT username FROM users "
        "WHERE id = :id"
        );

    query.bindValue(":id", userId);

    if (!query.exec()) {
        qWarning() << "Failed to query username:"
                   << query.lastError().text();
        return {};
    }

    if (!query.next()) {
        return {};
    }

    return query.value(0).toString();
}