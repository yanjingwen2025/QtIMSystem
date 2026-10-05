#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QStringListModel>
#include <QStringList>
#include <QListView>
#include <QPushButton>
#include <utility>
#include <QTimer>
#include <QDateTime>
#include <QStatusBar>
#include <QMessageBox>
#include <QFileDialog>

#include "network/tcpclient.h"
#include "network/filetransfermanager.h"

class TcpClient;
class FileTransferManager;

MainWindow::MainWindow(TcpClient *client,
                       FileTransferManager *fileManager,
                       QWidget *parent)
    : QMainWindow(parent),
    ui(new Ui::MainWindow),
    m_client(client),
    m_friendModel(new QStringListModel(this)),
    m_fileManager(fileManager)
{
    ui->setupUi(this);

    connect(m_client,
            &TcpClient::friendListReceived,
            this,
            [this](const QStringList &friends)
            {
                m_friends = friends;

                refreshFriendList();

                qDebug() << "Friend list received:"
                         << friends;
            });

    connect(m_client,
            &TcpClient::messageCreated,
            this,
            &MainWindow::onMessageCreated);

    connect(m_client,
            &TcpClient::messageStateChanged,
            this,
            &MainWindow::onMessageStateChanged);

    refreshFriendList();

    ui->friendListView->setModel(m_friendModel);

    connect(ui->friendListView,
            &QListView::clicked,
            this,
            &MainWindow::onFriendClicked);

    connect(ui->sendButton,
            &QPushButton::clicked,
            this,
            &MainWindow::onSendClicked);

    connect(m_client,
            &TcpClient::privateMessageReceived,
            this,
            &MainWindow::onPrivateMessageReceived);

    connect(ui->attachButton, &QPushButton::clicked,
            this, &MainWindow::onAttachClicked);

    connect(m_fileManager,
            &FileTransferManager::fileRequestArrived,
            this,
            &MainWindow::onFileRequestArrived);

    connect(m_fileManager,
            &FileTransferManager::sendProgress,
            this,
            [this](const QString &, qint64 done, qint64 total) {
                int percent = total > 0 ? int(done * 100 / total) : 0;
                statusBar()->showMessage(
                    QString("发送中: %1 / %2 (%3%)")
                        .arg(done).arg(total).arg(percent), 2000);
            });

    connect(m_fileManager,
            &FileTransferManager::receiveProgress,
            this,
            [this](const QString &, qint64 done, qint64 total) {
                int percent = total > 0 ? int(done * 100 / total) : 0;
                statusBar()->showMessage(
                    QString("接收中: %1 / %2 (%3%)")
                        .arg(done).arg(total).arg(percent), 2000);
            });

    connect(m_fileManager,
            &FileTransferManager::sendCompleted,
            this,
            [this](const QString &) {
                statusBar()->showMessage("文件发送完成", 3000);
            });

    connect(m_fileManager,
            &FileTransferManager::receiveCompleted,
            this,
            [this](const QString &) {
                statusBar()->showMessage("文件接收完成", 3000);
            });

    connect(m_fileManager,
            &FileTransferManager::sendFailed,
            this,
            [this](const QString &, const QString &reason) {
                QMessageBox::warning(this, "发送失败", reason);
            });

    connect(m_fileManager,
            &FileTransferManager::receiveFailed,
            this,
            [this](const QString &, const QString &reason) {
                QMessageBox::warning(this, "接收失败", reason);
            });
}




void MainWindow::onFriendClicked(const QModelIndex &index)
{
    if (!index.isValid()) {
        return;
    }

    int row = index.row();

    if (row < 0 || row >= m_friends.size()) {
        return;
    }

    QString friendName = m_friends.at(row);

    m_currentFriend = friendName;

    m_unreadCount[friendName] = 0;

    refreshFriendList();
    refreshChatHistory();

    ui->chatTitleLabel->setText(
        "正在与 " + friendName + " 聊天"
        );
}

void MainWindow::onSendClicked()
{
    if (m_currentFriend.isEmpty()) {
        ui->chatTitleLabel->setText("请先选择好友");
        return;
    }

    QString content = ui->messageEdit->toPlainText().trimmed();

    if (content.isEmpty()) {
        return;
    }

    m_client->sendPrivateMessage(m_currentFriend, content);

    //QString displayMessage =
        "我：" + content;

    //m_chatHistory[m_currentFriend].append(displayMessage);

    //ui->messageListWidget->addItem(displayMessage);

    //ui->messageEdit->clear();
}

void MainWindow::onPrivateMessageReceived(
    const QString &from,
    const QString &content)
{
    StoredMessage msg;
    msg.clientMsgId = "";   // 收到的消息没有本地 clientMsgId
    msg.sender = from;
    msg.content = content;
    msg.state = MessageStatus::Delivered;   // 收到的消息天然是"已送达"

    m_chatHistory[from].append(msg);

    if (from == m_currentFriend) {
        ui->messageListWidget->addItem(
            from + "：" + content
            );
        ui->messageListWidget->scrollToBottom();
    } else {
        m_unreadCount[from]++;
        refreshFriendList();
    }
}

void MainWindow::refreshChatHistory()
{
    ui->messageListWidget->clear();

    if (m_currentFriend.isEmpty()) {
        return;
    }

    const QVector<StoredMessage> &msgs =
        m_chatHistory[m_currentFriend];

    for (const StoredMessage &msg : msgs) {
        QString text = msg.sender + "：" + msg.content;

        // 只有"我"发的消息才显示状态
        if (msg.sender == "我") {
            text += stateSuffix(msg.state);
        }

        auto *item = new QListWidgetItem(text);

        if (!msg.clientMsgId.isEmpty()) {
            item->setData(Qt::UserRole, msg.clientMsgId);
            item->setData(Qt::UserRole + 1, msg.content);
        }

        ui->messageListWidget->addItem(item);
    }

    ui->messageListWidget->scrollToBottom();
}

void MainWindow::refreshFriendList()
{

    QStringList displayList;

    for (const QString &friendName : std::as_const(m_friends)) {
        int unread = m_unreadCount.value(friendName, 0);

        if (unread > 0) {
            displayList << QString("%1 (%2)")
            .arg(friendName)
                .arg(unread);
        } else {
            displayList << friendName;
        }
    }

    m_friendModel->setStringList(displayList);
}


void MainWindow::onMessageCreated(const QString &clientMsgId,
                                  const QString &to,
                                  const QString &content)
{
    StoredMessage msg;
    msg.clientMsgId = clientMsgId;
    msg.sender = "我";
    msg.content = content;
    msg.state = MessageStatus::Pending;

    m_chatHistory[to].append(msg);

    // 只有当前会话才显示到 UI
    if (to != m_currentFriend) {
        return;
    }

    auto *item = new QListWidgetItem(
        "我：" + content + stateSuffix(msg.state)
        );

    item->setData(Qt::UserRole, clientMsgId);

    ui->messageListWidget->addItem(item);

    ui->messageListWidget->scrollToBottom();
}


void MainWindow::onMessageStateChanged(const QString &clientMsgId,
                                       MessageStatus state)
{
    // 1. 找到这条消息在哪个会话里
    //    为了方便，直接遍历所有会话更新数据结构
    for (auto it = m_chatHistory.begin();
         it != m_chatHistory.end();
         ++it)
    {
        QVector<StoredMessage> &msgs = it.value();

        for (StoredMessage &msg : msgs) {
            if (msg.clientMsgId == clientMsgId) {
                msg.state = state;
            }
        }
    }

    // 2. 如果当前 UI 里有这条消息，更新它
    QListWidgetItem *item =
        findItemByClientMsgId(clientMsgId);

    if (item == nullptr) {
        return;
    }

    // 需要找到 content 才能重写文本
    // 简单做法：直接从 item 里拿旧文本，替换后缀
    QString text = item->text();

    // 去掉旧的后缀（如 " [发送中]"）
    // 更稳的做法是从 m_chatHistory 里找，但这里为了简单：
    // 我们约定文本格式是 "我：<content> [状态]"
    // 所以用 QRegularExpression 或手动 split 都可以

    // 简单且稳的做法：从 data 里拿 clientMsgId，
    // 从 m_chatHistory 里反查 content
    QString content;

    for (auto it = m_chatHistory.constBegin();
         it != m_chatHistory.constEnd();
         ++it)
    {
        for (const StoredMessage &msg : it.value()) {
            if (msg.clientMsgId == clientMsgId) {
                content = msg.content;
                break;
            }
        }
        if (!content.isEmpty()) break;
    }

    item->setText(
        "我：" + content + stateSuffix(state)
        );
}

QListWidgetItem *MainWindow::findItemByClientMsgId(
    const QString &clientMsgId)
{
    for (int i = 0; i < ui->messageListWidget->count(); ++i) {
        QListWidgetItem *item =
            ui->messageListWidget->item(i);

        if (item->data(Qt::UserRole).toString() == clientMsgId) {
            return item;
        }
    }

    return nullptr;
}

QString MainWindow::stateSuffix(MessageStatus state)
{
    switch (state) {
    case MessageStatus::Pending:   return " [发送中]";
    case MessageStatus::Sent:      return " [已发送]";
    case MessageStatus::Delivered: return " [已送达]";
    case MessageStatus::Read:      return " [已读]";
    case MessageStatus::Failed:    return " [发送失败]";
    }

    return {};
}

void MainWindow::onAttachClicked()
{
    if (m_currentFriend.isEmpty()) {
        QMessageBox::information(this, "提示",
                                 "请先选择一个好友");
        return;
    }

    QString path = QFileDialog::getOpenFileName(
        this,
        "选择要发送的文件",
        QString(),
        "所有文件 (*.*)"
        );

    if (path.isEmpty()) {
        return;
    }

    m_fileManager->sendFile(m_currentFriend, path);

    statusBar()->showMessage(
        QString("已请求发送: %1").arg(path), 3000);
}

void MainWindow::onFileRequestArrived(
    const QString &from,
    const FileMeta &meta)
{
    QString text = QString(
                       "%1 想发文件给你：\n\n"
                       "文件名: %2\n"
                       "大小: %3 字节\n\n"
                       "是否接收？"
                       ).arg(from)
                       .arg(meta.fileName)
                       .arg(meta.fileSize);

    auto result = QMessageBox::question(
        this,
        "文件传输请求",
        text,
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::Yes
        );

    if (result == QMessageBox::Yes) {
        m_fileManager->acceptFile(meta.fileId);
    } else {
        m_fileManager->rejectFile(meta.fileId);
    }
}



MainWindow::~MainWindow()
{
    delete ui;
}