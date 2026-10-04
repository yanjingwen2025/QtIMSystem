#include <QCoreApplication>
#include <QDebug>

#include "database/databasemanager.h"
#include "network/tcpserver.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    DatabaseManager database;

    if (!database.initialize()) {
        qCritical() << "Database initialization failed.";
        return -1;
    }

    if (database.addFriend("Alice", "Bob")) {
        qInfo() << "Test friendship ready: Alice <-> Bob";
    }

    qInfo() << "Alice friends:"
            << database.getFriends("Alice");

    qInfo() << "Bob friends:"
            << database.getFriends("Bob");

    TcpServer server(&database);

    if (!server.start(8888)) {
        return -1;
    }

    return app.exec();
}