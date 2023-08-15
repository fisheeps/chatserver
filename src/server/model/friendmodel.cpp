#include "friendmodel.hpp"
#include "db.hpp"

// 添加好友关系
void FriendModel::insert(int userid, int friendid)
{
    // 组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "insert into friend values(%d, %d)", userid, friendid);

    // 创建自定义数据库对象
    MySQL mysql;
    // 调用自定义链接数据库方法
    if (mysql.connect())
    {
        // 更新数据库
        mysql.update(sql);
    }
}

// 查询好友列表
vector<User> FriendModel::query(int userid)
{
    char sql[1024] = {0};
    sprintf(sql, "select a.id, a.name, a.state from user a inner join friend b on b.friendid  = a.id where b.userid = %d", userid);

    MySQL mysql;
    vector<User> vec;
    if (mysql.connect())
    {
        MYSQL_RES *res = mysql.query(sql);
        if (res != nullptr)
        {
            // MYSQL_ROW为指向指向数据库查询结果(字符串)指针的指针
            MYSQL_ROW row = mysql_fetch_row(res);
            if (row != nullptr)
            {
                User user;
                user.setId(atoi(row[0]));
                user.setName(row[1]);
                user.setState(row[2]);
                vec.push_back(user);
            }
        }
        mysql_free_result(res);
    }
    return vec;
}
