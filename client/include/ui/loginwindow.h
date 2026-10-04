#ifndef LOGINWINDOW_H
#define LOGINWINDOW_H

#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui {
class LoginWindow;
}
QT_END_NAMESPACE

class TcpClient;

class LoginWindow : public QWidget
{
    Q_OBJECT

public:
    explicit LoginWindow(TcpClient *client,
                         QWidget *parent = nullptr);
    ~LoginWindow();


signals:
    void loginSucceeded();

private slots:
    void onLoginClicked();
    void onRegisterClicked();

    void onLoginResult(bool success,
                       const QString &message);

    void onRegisterResult(bool success,
                          const QString &message);

    void onConnectionStatusChanged(bool connected);

private:
    Ui::LoginWindow *ui;
    TcpClient *m_client;
};

#endif // LOGINWINDOW_H