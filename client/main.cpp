#include "ui/loginwindow.h"
#include "ui/mainwindow.h"
#include "network/tcpclient.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // 整个客户端程序共用一个 TcpClient
    TcpClient client;

    LoginWindow loginWindow(&client);
    MainWindow mainWindow(&client);

    QObject::connect(
        &loginWindow,
        &LoginWindow::loginSucceeded,
        [&loginWindow, &mainWindow]()
        {
            loginWindow.hide();
            mainWindow.show();
        }
        );

    loginWindow.show();

    client.connectToServer("127.0.0.1", 8888);

    return app.exec();
}