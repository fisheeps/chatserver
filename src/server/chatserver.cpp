#include "chatserver.hpp"
#include "chatservice.hpp"
#include <functional>
#include <iostream>
using namespace std;
using namespace placeholders;

ChatServer::ChatServer(EventLoop *loop,
                       const InetAddress &listenAddr,
                       const string &nameArg) : _server(loop, listenAddr, nameArg), _loop(loop)
{
    // 注册连接回调函数
    _server.setConnectionCallback(std::bind(&ChatServer::onConnection, this, _1));

    // 注册消息处理回调函数
    _server.setMessageCallback(std::bind(&ChatServer::onMessage, this, _1, _2, _3));

    // 设置服务器端线程数
    _server.setThreadNum(4);
}

void ChatServer::start()
{
    _server.start();
}

void ChatServer::onConnection(const TcpConnectionPtr &conn)
{
    // 客户端断开连接
    if (!conn->connected())
    {
        // 异常退出修改用户的状态以及连接数据
        ChatService::GetInstance().clientCloseException(conn);
        conn->shutdown();
    }
}

void ChatServer::onMessage(const TcpConnectionPtr &conn,
                           Buffer *buffer,
                           Timestamp time)
{
    // 从缓冲区读取数据
    string readbuf = buffer->retrieveAllAsString();
    cout << readbuf << endl;

    // 数据反序列化
    json js = json::parse(readbuf);

    // 通过js["msgid"]获取一个业务处理器，然后有业务模块处理业务操作
    auto msgHandler = ChatService::GetInstance().gethandler(js["msgid"].get<int>());
    msgHandler(conn, js, time);
}