#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <atomic>
#pragma once
#pragma comment(lib, "Ws2_32.lib")

class Client {
private:
    std::atomic<SOCKET> s_server;
    u_short u_port;
    char* bufferSend;
    std::thread t_Recv;

    bool Init();

    bool setStartSocketSettings();

    void createThreadForRecv();

    void recvFunction();

public:
    std::atomic<bool> b_Work;
    Client(u_short port);
    ~Client();
    void Send(std::string& message);
};