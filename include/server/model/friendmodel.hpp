#ifndef FRIENDMODEL_H
#define FRIENDMODEL_H

#include <vector>
#include "user.hpp"
using namespace std;

class FriendModel
{
public:
    // 添加好友关系
    void insert(int userid, int friendid);
    // 返回用户好友关系
    vector<User> query(int userid);
};

#endif