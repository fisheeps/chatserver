#ifndef USERMODEL_H
#define USERMODEL_H

#include "user.hpp"

class UserModel
{
public:
    // 新增数据
    bool insert(User &user);
    // 查询数据
    User query(int id);
    // 更新登录状态
    bool updateState(User user);
    // 重置状态
    bool resetState();
};

#endif