#include <iostream>
#include "chatserver.hpp"
#include "chatservice.hpp"
#include <signal.h>
#include <iostream>
using namespace std;

void resethandler(int signal)
{
    ChatService::GetInstance().reset();
    exit(0);
}

int main(int argc, char const *argv[])
{
    if (argc < 3)
    {
        cerr << "command invalid! example: ./ChatServer 127.0.0.1 6000" << endl;
        exit(-1);
    }

    char const *ip = argv[1];
    uint16_t port = atoi(argv[2]);

    signal(SIGINT, resethandler);
    EventLoop loop;
    InetAddress addr(ip, port);
    ChatServer server(&loop, addr, "ChatServer");

    server.start();
    loop.loop();
    
    return 0;
}