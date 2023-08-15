#include <iostream>
#include <functional>
#include <string>
#include <vector>
#include <unordered_map>
#include <ctime>
#include <chrono>
#include "json.hpp"
using namespace std;
using json = nlohmann::json;

// 导入ORM类和枚举类
#include "user.hpp"
#include "group.hpp"
#include "public.hpp"

// 导入网络编程依赖库
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

// 导入多线程依赖库
#include <semaphore.h>
#include <atomic>
#include <thread>

// 全局变量
User m_currentUser;                   // 当前用户信息
vector<User> m_currentUserFriendList; // 当前用户好友信息
vector<Group> m_currentUserGroupList; // 当前用户群组信息
bool isMainMenuRunning = false;       // 页面控制标志位
sem_t rwsem;                          // 控制线程读写
atomic_bool m_isLoginSuccess{false};  // 记录登录状态

// 提前声明方法
void readTaskHandler(int clientfd); // 消息接受线程
string getCurrentTime();            // 获取系统时间
void mainMenu(int);                 // 主聊天界面
void showCurrentUserData();         // 显示当前用户的基本信息

/**
 * @brief 主函数入口
 * @param argc 参数个数
 * @param argv 参数列表,目标IP和目标port
 */
int main(int argc, char const *argv[])
{
    // 检查参数
    if (argc < 3)
    {
        // 标准错误输出(unbuffered),不会通过缓存,而是立刻输出
        cerr << "command invalid! example: ./ChatClient 127.0.0.1 6000" << endl;
        exit(-1);
    }

    // 解析参数并导入IP号和Port号
    const char *ip = argv[1];
    uint16_t port = atoi(argv[2]);

    // 创建client端socket
    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if (clientfd == -1)
    {
        cerr << "socket create error" << endl;
    }

    // 绑定目标主机地址和端口
    sockaddr_in server;
    memset(&server, 0, sizeof(sockaddr_in));

    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = inet_addr(ip);

    // 连接服务器
    if (connect(clientfd, (sockaddr *)&server, sizeof(sockaddr_in)) == -1)
    {
        cerr << "connect server error" << endl;
        close(clientfd);
        exit(-1);
    }

    // 初始化读写线程通信用的信号量
    sem_init(&rwsem, 0, 0);

    // 创建读取消息子线程，主线程负责发送消息,传入参数为通信端口号
    thread readTask(readTaskHandler, clientfd);
    readTask.detach();

    // 主线程循环
    while (true)
    {
        // 显示首页面菜单 登录、注册、退出
        cout << "========================" << endl;
        cout << "1. login" << endl;
        cout << "2. register" << endl;
        cout << "3. quit" << endl;
        cout << "========================" << endl;
        cout << "choice:";
        int choice = 0;
        cin >> choice;
        // 读取最后的回车
        cin.get();

        switch (choice)
        {
        case 1:
        {
            int id = 0;
            char pwd[50] = {0};
            cout << "usrid:";
            cin >> id;
            cin.get();
            cout << "userpassword:";
            cin.getline(pwd, 50);

            json js;
            js["msgid"] = EnMsgType::LOGIN_MSG;
            js["id"] = id;
            js["password"] = pwd;
            string request = js.dump();

            m_isLoginSuccess = false;

            int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
            if (len == -1)
            {
                cerr << "send login msg error:" << request << endl;
            }

            sem_wait(&rwsem); // 等待发送，子线程处理完登录响应后通知运行

            if (m_isLoginSuccess)
            {
                isMainMenuRunning = true;
                mainMenu(clientfd);
            }
        }
        break;
        case 2:
        {
            char name[50] = {0};
            char pwd[50] = {0};
            cout << "username:";
            cin.getline(name, 50);
            cout << "usrpassword:";
            cin.getline(pwd, 50);

            json js;
            js["msgid"] = EnMsgType::REG_MSG;
            js["name"] = name;
            js["password"] = pwd;
            string request = js.dump();

            int len = send(clientfd, request.c_str(), strlen(request.c_str()), 0);
            if (len == -1)
            {
                cerr << "send reg msg error" << request << endl;
            }
            sem_wait(&rwsem);
        }
        break;
        case 3:
        {
            close(clientfd);
            sem_destroy(&rwsem);
            exit(0);
        }
        default:
            cerr << "invalid input!" << endl;
            break;
        }
    }

    return 0;
}

// 登录消息应答函数
void doLoginResponse(json js)
{
    // 登录失败
    if (js["errno"].get<int>() != 0)
    {
        cerr << js["msg"] << endl;
        m_isLoginSuccess = false;
    }
    else
    {
        m_currentUser.setId(js["id"].get<int>());
        m_currentUser.setName(js["name"]);

        // 记录当前用户的好友信息
        if (js.contains("friends"))
        {
            // 初始化
            m_currentUserFriendList.clear();

            vector<string> vec = js["friends"];
            for (auto i : vec)
            {
                // json反序列化string
                json temp = json::parse(i);
                User user;
                user.setId(temp["id"].get<int>());
                user.setName(temp["name"]);
                user.setState(temp["state"]);
                m_currentUserFriendList.push_back(user);
            }
        }

        // 记录当前用户的群组消息
        if (js.contains("groups"))
        {
            // 初始化
            m_currentUserGroupList.clear();

            vector<string> vec = js["groups"];
            for (auto &i : vec)
            {
                json temp = json::parse(i);
                Group group;
                group.setId(temp["id"].get<int>());
                group.setName(temp["groupname"]);
                group.setDesc(temp["groupdesc"]);

                vector<string> vec1 = temp["users"];
                for (auto &userstr : vec1)
                {
                    GroupUser user;
                    json temp1 = json::parse(userstr);
                    user.setId(temp1["id"].get<int>());
                    user.setName(temp1["name"]);
                    user.setState(temp1["state"]);
                    user.setRole(temp1["role"]);
                    group.getUsers().push_back(user);
                }
                m_currentUserGroupList.push_back(group);
            }
        }

        // 显示登录用户基本信息
        showCurrentUserData();

        // 显示当前用户离线消息:个人聊天消息或群组消息
        if (js.contains("offlinemsg"))
        {
            vector<string> vec = js["offlinemsg"];
            for (auto &str : vec)
            {
                json temp = json::parse(str);
                if (temp["msgid"].get<int>() == EnMsgType::ONE_CHAT_MSG)
                {
                    cout << temp["time"].get<string>() << " [" << temp["id"] << "]" << temp["name"].get<string>()
                         << " said: " << temp["msg"].get<string>() << endl;
                }
                else
                {
                    cout << "群消息[" << temp["groupid"] << "]:" << temp["time"].get<string>() << " [" << temp["id"] << "]" << temp["name"].get<string>()
                         << " said: " << temp["msg"].get<string>() << endl;
                }
            }
        }
        // 更新登录状态
        m_isLoginSuccess = true;
    }
}

// 注册消息应答函数
void doRegResponse(json js)
{
    // 注册失败
    if (js["errno"].get<int>() != 0)
    {
        cerr << "name is already exist, register error!" << endl;
    }
    else
    {
        cout << "name register success, userid is " << js["id"] << ", do not forget it!" << endl;
    }
}

void readTaskHandler(int clientfd)
{
    while (true)
    {
        // 接受用户收到的消息
        char buffer[1024] = {0};
        int len = recv(clientfd, &buffer, sizeof(buffer), 0);
        // recv为阻塞接受，返回为0可能是被中断之后继续运行的值
        if (len == -1 || len == 0)
        {
            close(clientfd);
            exit(-1);
        }

        // 接收ChatServer转发的消息，反序列化为json
        json js = json::parse(buffer);
        int msgtype = js["msgid"].get<int>();
        if (msgtype == EnMsgType::ONE_CHAT_MSG)
        {
            cout << js["time"].get<string>() << " [" << js["id"] << "]" << js["name"].get<string>()
                 << " said: " << js["msg"].get<string>() << endl;
        }
        else if (msgtype == EnMsgType::GROUP_CHAT_MSG)
        {
            cout << "群消息[" << js["groupid"] << "]:" << js["time"].get<string>() << " [" << js["id"] << "]" << js["name"].get<string>()
                 << " said: " << js["msg"].get<string>() << endl;
        }
        else if (msgtype == EnMsgType::LOGIN_MSG_ACK)
        {
            doLoginResponse(js);
            sem_post(&rwsem);
        }
        else if (msgtype == EnMsgType::REG_MSG_ACK)
        {
            doRegResponse(js);
            sem_post(&rwsem);
        }
    }
}

// 获取当前系统时间
string getCurrentTime()
{
    auto tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    struct tm *ptm = localtime(&tt);
    char date[60] = {0};
    sprintf(date, "%d-%02d-%02d %02d:%02d:%02d",
            (int)ptm->tm_year + 1900, (int)ptm->tm_mon + 1, (int)ptm->tm_mday,
            (int)ptm->tm_hour, (int)ptm->tm_min, (int)ptm->tm_sec);
    return std::string(date);
}

// "help" command handler
void help(int fd = 0, string str = "");
// "chat" command handler
void chat(int, string);
// "addfriend" command handler
void addfriend(int, string);
// "creategroup" command handler
void creategroup(int, string);
// "addgroup" command handler
void addgroup(int, string);
// "groupchat" command handler
void groupchat(int, string);
// "loginout" command handler
void loginout(int, string);

// 系统支持的客户端命令列表
unordered_map<string, string> commandMap = {
    {"help", "显示所有支持的命令，格式help"},
    {"chat", "一对一聊天，格式chat:friendid:message"},
    {"addfriend", "添加好友，格式addfriend:friendid"},
    {"creategroup", "创建群组，格式creategroup:groupname:groupdesc"},
    {"addgroup", "加入群组，格式addgroup:groupid"},
    {"groupchat", "群聊，格式groupchat:groupid:message"},
    {"loginout", "注销，格式loginout"}};

// 注册系统支持的客户端命令处理
unordered_map<string, function<void(int, string)>> commandHandlerMap = {
    {"help", help},
    {"chat", chat},
    {"addfriend", addfriend},
    {"creategroup", creategroup},
    {"addgroup", addgroup},
    {"groupchat", groupchat},
    {"loginout", loginout}};

void mainMenu(int clientfd)
{
    help();

    char buffer[1024] = {0};
    while (isMainMenuRunning)
    {
        cin.getline(buffer, sizeof(buffer));
        string commandbuf(buffer);
        string command;
        // 寻找字符串中":"所在位置
        int idx = commandbuf.find(":");
        if (idx == -1)
        {
            command = commandbuf;
        }
        else
        {
            command = commandbuf.substr(0, idx);
        }
        auto it = commandHandlerMap.find(command);
        if (it == commandHandlerMap.end())
        {
            cerr << "invalid input command!" << endl;
            continue;
        }

        // 调用对应的时间处理回调函数
        it->second(clientfd, commandbuf.substr(idx + 1, commandbuf.size() - idx));
    }
}

void showCurrentUserData()
{
    cout << "======================login user======================" << endl;
    cout << "current login user => id:" << m_currentUser.getId() << " name:" << m_currentUser.getName() << endl;
    cout << "----------------------friend list---------------------" << endl;
    if (!m_currentUserFriendList.empty())
    {
        for (auto &user : m_currentUserFriendList)
        {
            cout << user.getId() << " " << user.getName() << " " << user.getState() << endl;
        }
    }
    cout << "----------------------group list-----------------------" << endl;
    if (!m_currentUserGroupList.empty())
    {
        for (auto &group : m_currentUserGroupList)
        {
            cout << group.getId() << " " << group.getName() << " " << group.getDesc() << endl;
            for (auto &user : group.getUsers())
            {
                cout << user.getId() << " " << user.getName() << " " << user.getState()
                     << " " << user.getRole() << endl;
            }
        }
    }
    cout << "======================================================" << endl;
}

void help(int fd, string str)
{
    cout << "show command list >>> " << endl;
    for (auto &p : commandMap)
    {
        cout << p.first << " : " << p.second << endl;
    }
    cout << endl;
}

void chat(int clientfd, string str)
{
    // 聊天格式 chat:friendid:msg
    int idx = str.find(":"); // friendid:message
    if (-1 == idx)
    {
        cerr << "chat command invalid!" << endl;
        return;
    }

    // 提取聊天信息
    int friendid = atoi(str.substr(0, idx).c_str());
    string message = str.substr(idx + 1, str.size() - idx);

    json js;
    js["msgid"] = ONE_CHAT_MSG;
    js["id"] = m_currentUser.getId();
    js["name"] = m_currentUser.getName();
    js["toid"] = friendid;
    js["msg"] = message;
    js["time"] = getCurrentTime();
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len)
    {
        cerr << "send chat msg error -> " << buffer << endl;
    }
}

void addfriend(int clientfd, string str)
{
    int friendid = atoi(str.c_str());
    json js;
    js["msgid"] = ADD_FRIEND_MSG;
    js["id"] = m_currentUser.getId();
    js["friendid"] = friendid;
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len)
    {
        cerr << "send addfriend msg error -> " << buffer << endl;
    }
}

void creategroup(int clientfd, string str)
{
    int idx = str.find(":");
    if (-1 == idx)
    {
        cerr << "creategroup command invalid!" << endl;
        return;
    }

    string groupname = str.substr(0, idx);
    string groupdesc = str.substr(idx + 1, str.size() - idx);

    json js;
    js["msgid"] = CREATE_GROUP_MSG;
    js["id"] = m_currentUser.getId();
    js["groupname"] = groupname;
    js["groupdesc"] = groupdesc;
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len)
    {
        cerr << "send creategroup msg error -> " << buffer << endl;
    }
}

void addgroup(int clientfd, string str)
{
    int groupid = atoi(str.c_str());
    json js;
    js["msgid"] = ADD_GROUP_MSG;
    js["id"] = m_currentUser.getId();
    js["groupid"] = groupid;
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len)
    {
        cerr << "send addgroup msg error -> " << buffer << endl;
    }
}

void groupchat(int clientfd, string str)
{
    int idx = str.find(":");
    if (-1 == idx)
    {
        cerr << "groupchat command invalid!" << endl;
        return;
    }

    int groupid = atoi(str.substr(0, idx).c_str());
    string message = str.substr(idx + 1, str.size() - idx);

    json js;
    js["msgid"] = GROUP_CHAT_MSG;
    js["id"] = m_currentUser.getId();
    js["name"] = m_currentUser.getName();
    js["groupid"] = groupid;
    js["msg"] = message;
    js["time"] = getCurrentTime();
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len)
    {
        cerr << "send groupchat msg error -> " << buffer << endl;
    }
}

void loginout(int clientfd, string str)
{
    json js;
    js["msgid"] = LOGINOUT_MSG;
    js["id"] = m_currentUser.getId();
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len)
    {
        cerr << "send loginout msg error -> " << buffer << endl;
    }
    else
    {
        isMainMenuRunning = false;
    }
}