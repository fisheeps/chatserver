#include "offlinemessagemodel.hpp"
#include "db.hpp"

// 添加离线消息
void OfflineMsgModel::insert(int userid, string msg)
{
    char sql[1024] = {0};
    sprintf(sql, "insert into offlinemessage values (%d, '%s')", userid, msg.c_str());

    // 添加到数据库
    MySQL mysql;
    if (mysql.connect())
    {
        mysql.update(sql);
    }
}

// 删除数据库元素
void OfflineMsgModel::remove(int userid)
{
    char sql[1024] = {0};
    sprintf(sql, "delete from offlinemessage where userid = %d", userid);

    // 添加到数据库
    MySQL mysql;
    if (mysql.connect())
    {
        mysql.update(sql);
    }
}

// 查询数据库元素
vector<string> OfflineMsgModel::query(int userid)
{
    vector<string> offlinemsg;
    char sql[1024] = {0};
    sprintf(sql, "select message from offlinemessage where userid = %d", userid);

    // 添加到数据库
    MySQL mysql;
    if (mysql.connect())
    {
        // MYSQL_RES数据类型为char**,即指向char*的指针
        // char* a[];
        // res = &a;
        MYSQL_RES *res = mysql.query(sql);
        if (res != nullptr)
        {
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(res)) != nullptr)
            {
                // 查询的时候只查了一项，所以读取第一位就行
                offlinemsg.push_back(row[0]);
            }
        }
        // query返回的是指针，是在mysql模块中创建并占用了内存地址
        // 由于将引用传递出来，在模块内部没有对分配的内存进行释放
        mysql_free_result(res); // 释放内存，调用接口传出指针，需要在外部进行释放，防止内存泄漏
    }
    return offlinemsg;
}
