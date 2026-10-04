#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QStringListModel>
#include <QStringList>
#include <QListView>
#include <QPushButton>
#include <utility>

#include "network/tcpclient.h"

MainWindow::MainWindow(TcpClient *client,
                       QWidget *parent)
    : QMainWindow(parent),
    ui(new Ui::MainWindow),
    m_client(client),
    m_friendModel(new QStringListModel(this))
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

    QString displayMessage =
        "我：" + content;

    m_chatHistory[m_currentFriend].append(displayMessage);

    ui->messageListWidget->addItem(displayMessage);

    ui->messageEdit->clear();
}

void MainWindow::onPrivateMessageReceived(
    const QString &from,
    const QString &content)
{
    QString displayMessage =
        from + "：" + content;

    m_chatHistory[from].append(displayMessage);

    if (from == m_currentFriend) {
        ui->messageListWidget->addItem(displayMessage);
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

    const QStringList &messages =
        m_chatHistory[m_currentFriend];

    ui->messageListWidget->addItems(messages);
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

MainWindow::~MainWindow()
{
    delete ui;
}