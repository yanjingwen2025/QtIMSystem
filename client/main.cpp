#include "ui/loginwindow.h"
#include "ui/mainwindow.h"
#include "network/tcpclient.h"
#include "network/filetransfermanager.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // 整个客户端程序共用一个 TcpClient
    TcpClient client;

    FileTransferManager fileManager(&client);

    LoginWindow loginWindow(&client);
    MainWindow mainWindow(&client, &fileManager);

    QObject::connect(
        &loginWindow,
        &LoginWindow::loginSucceeded,
        [&loginWindow, &mainWindow]()
        {
            loginWindow.hide();
            mainWindow.show();
        }
        );

    // 阶段 4-6 临时：alice 登录后自动发文件
    /*
    QObject::connect(
        &loginWindow,
        &LoginWindow::loginSucceededAs,
        &fileManager,
        [&fileManager](const QString &username) {
            if (username == "Alice") {
                fileManager.sendFile(
                    "Bob",
                    "F:/test_send_huge.bin"
                    );
            }
        }
        );


    // 阶段 4-6 临时：自动接受文件
    QObject::connect(
        &fileManager,
        &FileTransferManager::fileRequestArrived,
        &fileManager,
        [&fileManager](const QString &from,
                       const FileMeta &meta) {

            qInfo() << "[main] Auto-accept file from"
                    << from
                    << meta.fileName;

            fileManager.acceptFile(meta.fileId);
        }
        );
    */
    loginWindow.show();

    client.connectToServer("127.0.0.1", 8888);

    return app.exec();
}