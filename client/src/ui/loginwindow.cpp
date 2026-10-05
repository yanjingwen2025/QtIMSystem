#include "loginwindow.h"
#include "ui_loginwindow.h"

#include "network/tcpclient.h"

#include <QLineEdit>
#include <QPushButton>

LoginWindow::LoginWindow(TcpClient *client,
                         QWidget *parent)
    : QWidget(parent),
    ui(new Ui::LoginWindow),
    m_client(client)
{
    ui->setupUi(this);

    connect(ui->loginButton,
            &QPushButton::clicked,
            this,
            &LoginWindow::onLoginClicked);

    connect(ui->registerButton,
            &QPushButton::clicked,
            this,
            &LoginWindow::onRegisterClicked);

    connect(m_client,
            &TcpClient::loginResult,
            this,
            &LoginWindow::onLoginResult);

    connect(m_client,
            &TcpClient::registerResult,
            this,
            &LoginWindow::onRegisterResult);

    connect(m_client,
            &TcpClient::connectionStatusChanged,
            this,
            &LoginWindow::onConnectionStatusChanged);
}

LoginWindow::~LoginWindow()
{
    delete ui;
}

void LoginWindow::onLoginClicked()
{


    QString username =
        ui->usernameEdit->text().trimmed();

    QString password =
        ui->passwordEdit->text();

    if (username.isEmpty() || password.isEmpty()) {
        ui->statusLabel->setText("请输入用户名和密码");
        return;
    }

    m_lastLoginUsername = username;

    ui->statusLabel->setText("正在登录...");

    m_client->login(username, password);


}

void LoginWindow::onRegisterClicked()
{
    QString username =
        ui->usernameEdit->text().trimmed();

    QString password =
        ui->passwordEdit->text();

    if (username.isEmpty() || password.isEmpty()) {
        ui->statusLabel->setText("请输入用户名和密码");
        return;
    }

    ui->statusLabel->setText("正在注册...");

    m_client->registerUser(username, password);
}

void LoginWindow::onLoginResult(bool success,
                                const QString &message)
{
    if (success) {
        ui->statusLabel->setText("登录成功");
        m_client->requestFriendList();
        emit loginSucceeded();
        emit loginSucceededAs(m_lastLoginUsername);
    } else {
        ui->statusLabel->setText(
            "登录失败：" + message
            );
    }
}

void LoginWindow::onRegisterResult(bool success,
                                   const QString &message)
{
    if (success) {
        ui->statusLabel->setText("注册成功");
    } else {
        ui->statusLabel->setText(
            "注册失败：" + message
            );
    }
}

void LoginWindow::onConnectionStatusChanged(bool connected)
{
    if (connected) {
        ui->statusLabel->setText("状态：已连接服务器");
    } else {
        ui->statusLabel->setText("状态：服务器已断开");
    }
}