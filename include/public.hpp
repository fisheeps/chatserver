#ifndef PUBLIC_H
#define PUBLIC_H

// 消息类型
enum EnMsgType
{
    LOGIN_MSG = 1,    // 登录消息
    LOGIN_MSG_ACK,    // 登录应答
    LOGINOUT_MSG,     // 注销消息
    REG_MSG,          // 注册消息
    REG_MSG_ACK,      // 注册应答
    ONE_CHAT_MSG = 6, // 聊天消息
    ADD_FRIEND_MSG,   // 添加好友消息
    CREATE_GROUP_MSG, // 创建群组
    ADD_GROUP_MSG,    // 加入群组
    GROUP_CHAT_MSG,   // 群聊天
};

#endif