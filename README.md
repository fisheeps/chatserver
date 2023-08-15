# chatserver
可以工作在Nginx TCP负载均衡环境中的集群聊天服务器和客户端源码 基于muduo实现 使用redis作为订阅发布中间件

- GCC版本：7.5.0
- nginx版本：1.12.2,该版本的nginx在安装过程中会报错，提示有成员已经没有使用，找到对应文件，通过vim打开文件，文件路径为解压文件目录下的src/os/unix/ngx_user.c,找到"cd.current_salt[0] = ~salt[0];"并注释即可

## 编译方式
- mkdir build
- cd build
- cmake ..

### 使用编译脚本
- 修改脚本权限:chomd -x autobuild.sh
- 运行脚本文件:./autobuild.sh