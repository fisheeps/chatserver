#include "thirdparty/json.hpp"
#include <iostream>
#include <vector>
#include <map>
using json = nlohmann::json;
using namespace std;

// 数据序列化
string func1()
{
    json js;
    js["usr name"] = "zhang san";
    js["id"] = {1, 2, 3, 4, 5};
    js["msg"]["zhang san"] = "hello world";
    js["msg"]["liu shuo"] = "hello china";
    string sendBuf = js.dump();
    // cout << sendBuf.c_str() << endl;
    return sendBuf;
}

// 容器序列化
string func2()
{
    json js;
    vector<int> vec;
    vec.push_back(1);
    vec.push_back(2);
    vec.push_back(3);
    js["list"] = vec;

    map<int, string> m;
    m.insert({1, "黄山"});
    m.insert({2, "华山"});
    m.insert({3, "泰山"});
    js["path"] = m;

    string sendBuf = js.dump();
    // cout << sendBuf.c_str() << endl;
    return sendBuf;
}

int main(int argc, char const *argv[])
{
    // 反序列化
    json jsbuf = json::parse(func1());
    cout << jsbuf << endl;

    jsbuf = json::parse(func2());
    vector<int> vec = jsbuf["list"];
    for (int val : vec)
    {
        cout << val << " ";
    }
    cout << endl;

    map<int, string> m = jsbuf["path"];
    for (auto p : m)
    {
        cout << p.first << " " << p.second << endl;
    }
    cout << endl;

    return 0;
}
