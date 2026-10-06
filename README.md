## 一、分层架构图
<img width="1055" height="1491" alt="ChatGPT Image 2026年10月6日 13_44_59" src="https://github.com/user-attachments/assets/e2a49e7e-7c7d-4c77-94ae-7942d4b74545" />
### 1、简介：
QtIMSystem 采用 Client/Server 架构，客户端和服务端都是 Qt 程序，通过自定义 TCP 长连接协议通信，数据库只在服务端，客户端之间不直接通信，所有消息经服务端中转。客户端分 4 层，层间通过信号槽解耦，业务逻辑主要分布在 TcpClient 和 MainWindow 中，文件传输因为复杂度高单独抽成了 FileTransferManager。客户端和服务端之间使用自定义应用层协议，Magic：协议标识，防伪造，Type：MessageType 枚举，用于分发，BodyLength：解决 TCP 粘包 / 半包，Body：JSON 格式的业务数据。
<img width="2172" height="724" alt="image" src="https://github.com/user-attachments/assets/9ddc7974-c37f-4b5a-9dc4-fe62a9f1a0aa" />

### 2、下一步改进方向：
#### 2.1 业务层独立：
当前，认证、聊天、连接管理三块业务，目前分散在 TcpClient 和 MainWindow 中
只有文件传输独立成了 FileTransferManager，下一步可将其抽象出Service层，将业务逻辑从各层抽出来，实现UI 层和业务层解耦。
#### 2.2 Model/View ，数据与显示分离：
当前，好友列表用 QStringListModel，消息列表用 QListWidget + Qt::UserRole 存 clientMsgId，每次状态变化要遍历所有 item 查找，后续，好友列表：自定义 FriendListModel (QAbstractListModel)， 消息列表：QListView + MessageListModel + QStyledItemDelegate， 消息数据结构扩展：时间戳、方向、类型、状态，状态更新使用 dataChanged() 精确通知 View。
