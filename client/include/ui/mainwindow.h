#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QModelIndex>
#include <QHash>
#include <QStringList>
#include <QVector>
#include <QListWidgetItem>
#include "common/messagestatus.h"
#include "common/filemeta.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class TcpClient;
class QStringListModel;
class FileTransferManager;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(TcpClient *client,
                        FileTransferManager *fileManager,
                        QWidget *parent = nullptr);
    ~MainWindow();



private slots:
    void onFriendClicked(const QModelIndex &index);
    void onSendClicked();
    void onPrivateMessageReceived(const QString &from,
                                  const QString &content);

    void onMessageCreated(const QString &clientMsgId,
                          const QString &to,
                          const QString &content);

    void onMessageStateChanged(const QString &clientMsgId,
                               MessageStatus state);

    void onAttachClicked();
    void onFileRequestArrived(const QString &from,
                              const FileMeta &meta);

private:
    Ui::MainWindow *ui;
    TcpClient *m_client;

    QStringListModel *m_friendModel;

    QString m_currentFriend;

    FileTransferManager *m_fileManager;

    struct StoredMessage
    {
        QString clientMsgId;
        QString sender;        // "我" 或 好友名
        QString content;
        MessageStatus state;
    };

    QHash<QString, QVector<StoredMessage>> m_chatHistory;

    void refreshChatHistory();

    QHash<QString, int> m_unreadCount;

    void refreshFriendList();

    QStringList m_friends;

    QListWidgetItem *findItemByClientMsgId(
        const QString &clientMsgId);

    static QString stateSuffix(MessageStatus state);
};

#endif // MAINWINDOW_H