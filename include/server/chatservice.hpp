#ifndef CHATSERVICE_H
#define CHATSERVICE_H

#include <unordered_map>
#include <functional>
#include <muduo/net/TcpConnection.h>
#include "usermodel.hpp"
#include "offlinemessagemodel.hpp"
#include "friendmodel.hpp"
#include "groupmodel.hpp"
#include <mutex>
#include "json.hpp"
#include "redis.hpp"
using namespace std;
using namespace muduo;
using namespace muduo::net;
using json = nlohmann::json;

// 处理消息的事件回调方法类型(函数对象类型别名)
using MsgHandler = std::function<void(const TcpConnectionPtr &conn, json &js, Timestamp time)>;

/*
    业务类
    实现登录、注册、聊天等各种业务，提供网络模块使用的回调函数
    网络模块收到消息，解析消息类型，根据msgid，选择业务模块相应的回调函数
    业务类不同方法处理不同业务，涉及到数据库部分操作的，调用数据库model模块
    数据库model包括对数据库进行的增删改查操作，完成操作需要调用db中的自定义MySQL类
    自定义MySQL类包括基本的数据库操作，即连接数据库、更新数据库、查询数据库，这些方法是通过MYSQL提供的基本调用接口实现的
    由于直接编写SQL语句对数据库进行操作不方便，因此设计数据库表对应的ORM类对数据进行结构化存储
*/
class ChatService
{
public:
    static ChatService &GetInstance();

    // 登录业务
    void login(const TcpConnectionPtr &conn, json &js, Timestamp time);
    // 注册业务
    void reg(const TcpConnectionPtr &conn, json &js, Timestamp time);
    // 聊天方法
    void oneChat(const TcpConnectionPtr &conn, json &js, Timestamp time);
    // 添加好友方法
    void addFriend(const TcpConnectionPtr &conn, json &js, Timestamp time);
    // 创建群组业务
    void createGroup(const TcpConnectionPtr &conn, json &js, Timestamp time);
    // 加入群组业务
    void addGroup(const TcpConnectionPtr &conn, json &js, Timestamp time);
    // 群组聊天业务
    void groupChat(const TcpConnectionPtr &conn, json &js, Timestamp time);
    // 处理注销业务
    void loginout(const TcpConnectionPtr &conn, json &js, Timestamp time);

    // 获取消息对应的处理器
    MsgHandler gethandler(int msgid);
    // 处理用户异常关闭
    void clientCloseException(const TcpConnectionPtr &conn);
    // 重置所有用户状态
    void reset();

    // 从redis消息队列中获取订阅的消息
    void handlerRedisSubscribeMessage(int, string);

private:
    // 构造函数设置为private，使得外部程序无法调用new来实例化ChatService对象
    ChatService();

    // 存储消息id和其对应的处理方法
    unordered_map<int, MsgHandler> _msgHandlerMap;

    // 存储在线用户的通信连接
    unordered_map<int, TcpConnectionPtr> _userConnMap;

    // 互斥量，保证_userConnMap的线程安全
    mutex _connMutex;

    UserModel _usermodel;          // Usermodel对象
    OfflineMsgModel _offlinemodel; // OfflineMsgModel对象
    FriendModel _friendmodel;      // FriendModel对象
    GroupModel _groupmodel;        // GroupModel对象

    Redis _redis; // redis操作对象
};

#endif