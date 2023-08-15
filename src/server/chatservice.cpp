#include "chatservice.hpp"
#include "public.hpp"
#include <muduo/base/Logging.h>
#include <iostream>
using namespace muduo;

// 单例模式
ChatService &ChatService::GetInstance()
{
    static ChatService chatservice;
    return chatservice;
}

// 用户登录方法
void ChatService::login(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int id = js["id"].get<int>();
    string password = js["password"];

    json response;
    User user = _usermodel.query(id);
    if (id == user.getId() && password == user.getPwd())
    {
        // 存在用户
        // 查看用户状态，是否已经登录
        if (user.getState() == "online")
        {
            // 用户已经登录
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 2;
            response["errmsg"] = "当前用户已登录，请重新输入账号。";
        }
        else
        {
            // 记录用户的通信连接,限制互斥操作在最小作用域
            {
                lock_guard<mutex> lock(_connMutex);
                _userConnMap.insert({user.getId(), conn});
            }

            // 订阅用户消息
            _redis.subscribe(user.getId());

            // 用户未登录，修改用户登录状态，并返回登录成功信息
            user.setState("online");
            _usermodel.updateState(user);
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 0;
            response["id"] = user.getId();
            response["name"] = user.getName();

            // 登录成功，查看是否有离线消息，如果有则读取并返回离线消息
            vector<string> vec = _offlinemodel.query(user.getId());
            if (!vec.empty())
            {
                // 有离线消息，读取
                response["offlinemsg"] = vec;
                _offlinemodel.remove(user.getId());
            }

            // 输出好友列表
            vector<User> vec1 = _friendmodel.query(user.getId());
            if (!vec1.empty())
            {
                vector<string> vec2;
                for (auto i : vec1)
                {
                    json temp;
                    temp["id"] = i.getId();
                    temp["name"] = i.getName();
                    temp["state"] = i.getState();
                    vec2.push_back(temp.dump());
                }
                response["friends"] = vec2;
            }
        }
    }
    else
    {
        // 不存在用户，返回错误信息
        response["msgid"] = LOGIN_MSG_ACK;
        response["errno"] = 1;
        response["errmsg"] = "当前用户不存在。";
    }
    conn->send(response.dump());
}

// 注册业务
void ChatService::reg(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    string name = js["name"];
    string password = js["password"];

    User user;
    user.setName(name);
    user.setPassword(password);
    json response;
    if (_usermodel.insert(user))
    {
        // 注册成功，返回对应状态
        response["msgid"] = EnMsgType::REG_MSG_ACK; // 注册成功应答
        response["errno"] = 0;                      // 0表示未出错
        response["id"] = user.getId();              // 返回用户在列表中对应的ID号
        conn->send(response.dump());
    }
    else
    {
        // 注册失败
        response["msgid"] = EnMsgType::REG_MSG_ACK;
        response["errno"] = 1;
        conn->send(response.dump());
    }
}

// 聊天方法
void ChatService::oneChat(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    // 获取目标的ID号
    int toid = js["toid"].get<int>();
    // 查询当前ID是否在线
    {
        // 遍历已连接列表，要做好线程保护
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(toid);
        if (it != _userConnMap.end())
        {
            it->second->send(js.dump());
            return;
        }
    }

    // 查询用户数据库的用户状态,看是否在其他服务器上
    User user = _usermodel.query(toid);
    if (user.getState() == "online")
    {
        _redis.publish(user.getId(), js.dump());
    }

    // 用户不在线，将消息写入离线消息数据库中
    _offlinemodel.insert(toid, js.dump());
}

// 添加好友方法
void ChatService::addFriend(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int id = js["id"].get<int>();
    int friendid = js["friendid"].get<int>();

    _friendmodel.insert(id, friendid);
}

void ChatService::createGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    string name = js["groupname"];
    string desc = js["groupdesc"];

    // 存储新创建的群组信息
    Group group(-1, name, desc);
    if (_groupmodel.createGroup(group))
    {
        // 存储群组创建人信息
        _groupmodel.addGroup(userid, group.getId(), "creator");
    }
}

void ChatService::addGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    _groupmodel.addGroup(userid, groupid, "normal");
}

void ChatService::groupChat(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    vector<int> useridVec = _groupmodel.queryGroupUsers(userid, groupid);

    lock_guard<mutex> lock(_connMutex);
    for (int id : useridVec)
    {
        auto it = _userConnMap.find(id);
        if (it != _userConnMap.end())
        {
            // 转发群消息
            it->second->send(js.dump());
        }
        else
        {
            // 查询toid是否在线
            User user = _usermodel.query(id);
            if (user.getState() == "online")
            {
                _redis.publish(id, js.dump());
            }
            else
            {
                // 存储离线群消息
                _offlinemodel.insert(id, js.dump());
            }
        }
    }
}

void ChatService::loginout(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"];

    // 将用户从已连接的列表中移除
    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(userid);
        if (it != _userConnMap.end())
        {
            _userConnMap.erase(it);
        }
    }

    // 取消用户的redis订阅
    _redis.unsubscribe(userid);

    // 更新用户的状态信息
    User user(userid, "", "", "offline");
    _usermodel.updateState(user);
}

// 获取函数对象
MsgHandler ChatService::gethandler(int msgid)
{
    // 判断是否存在该ID号
    if (_msgHandlerMap.find(msgid) == _msgHandlerMap.end())
    {
        // 不存在，返回临时函数对象，该函数对象进行错误日志输出
        return [=](const TcpConnectionPtr &, json &, Timestamp)
        {
            LOG_ERROR << "msgid:" << msgid << "can not find!";
        };
    }
    else
    {
        return _msgHandlerMap[msgid];
    }
}

void ChatService::clientCloseException(const TcpConnectionPtr &conn)
{
    // 用户异常连接，根据conn找到用户的id
    User user;
    {
        // 删除操作可能会与加入操作冲突，为保证线程安全，需要进行线程保护
        // 由于增删元素会导致迭代器失效，所以必须对整个遍历for循环进行线程保护
        lock_guard<mutex> lock(_connMutex);
        for (auto i = _userConnMap.begin(); i != _userConnMap.end(); i++)
        {
            if (i->second == conn)
            {
                user.setId(i->first);
                // 将异常连接列表从已连接列表删除
                _userConnMap.erase(i);
            }
        }
    }

    // 取消用户的redis订阅
    _redis.unsubscribe(user.getId());

    // 将用户的状态修改为offline
    if (user.getId() != -1)
    {
        user.setState("offline");
        _usermodel.updateState(user);
    }
}

// 重置用户状态
void ChatService::reset()
{
    _usermodel.resetState();
}

void ChatService::handlerRedisSubscribeMessage(int id, string msg)
{
    // 查询id对应的用户是否还在线
    lock_guard<mutex> lock(_connMutex);
    auto it = _userConnMap.find(id);
    if (it != _userConnMap.end())
    {
        it->second->send(msg);
    }

    // 如果没有找到则说明在消息发布期间用户已经下线，将消息存入离线消息数据库中
    _offlinemodel.insert(id, msg);
}

ChatService::ChatService()
{
    _msgHandlerMap.insert({EnMsgType::LOGIN_MSG, std::bind(&ChatService::login, this, _1, _2, _3)});
    _msgHandlerMap.insert({EnMsgType::LOGINOUT_MSG, std::bind(&ChatService::loginout, this, _1, _2, _3)});
    _msgHandlerMap.insert({EnMsgType::REG_MSG, std::bind(&ChatService::reg, this, _1, _2, _3)});
    _msgHandlerMap.insert({EnMsgType::ONE_CHAT_MSG, std::bind(&ChatService::oneChat, this, _1, _2, _3)});
    _msgHandlerMap.insert({EnMsgType::ADD_FRIEND_MSG, std::bind(&ChatService::addFriend, this, _1, _2, _3)});

    // 群组业务管理相关事件处理回调注册
    _msgHandlerMap.insert({EnMsgType::CREATE_GROUP_MSG, std::bind(&ChatService::createGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert({EnMsgType::ADD_GROUP_MSG, std::bind(&ChatService::addGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert({EnMsgType::GROUP_CHAT_MSG, std::bind(&ChatService::groupChat, this, _1, _2, _3)});

    // 连接redis服务
    if (_redis.connect())
    {
        // 注册回调函数,传入两个参数、订阅号-ID号、消息内容
        _redis.init_notify_handler(std::bind(&ChatService::handlerRedisSubscribeMessage, this, _1, _2));
    }
}