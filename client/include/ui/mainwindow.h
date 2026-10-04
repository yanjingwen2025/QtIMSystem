#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QModelIndex>
#include <QHash>
#include <QStringList>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class TcpClient;
class QStringListModel;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(TcpClient *client,
                        QWidget *parent = nullptr);
    ~MainWindow();



private slots:
    void onFriendClicked(const QModelIndex &index);
    void onSendClicked();
    void onPrivateMessageReceived(const QString &from,
                                  const QString &content);

private:
    Ui::MainWindow *ui;
    TcpClient *m_client;

    QStringListModel *m_friendModel;

    QString m_currentFriend;

    QHash<QString, QStringList> m_chatHistory;

    void refreshChatHistory();

    QHash<QString, int> m_unreadCount;

    void refreshFriendList();

    QStringList m_friends;
};

#endif // MAINWINDOW_H