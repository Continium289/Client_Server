#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <vector>
#include <map>
#include <atomic>
#include <csignal>
#include <mutex>

#pragma once
#pragma comment(lib, "Ws2_32.lib")

class Server {
private:
    std::atomic<SOCKET> s_listening;
    u_short u_port;
    std::thread t_Accept;
    std::thread t_CheckActual;
    std::thread t_SendToAll;
    std::vector<std::thread> v_threads;
    std::vector<std::thread::id> v_threadsNumsEnd;
    std::map<std::thread::id, SOCKET> m_sockets;
    std::mutex m_main;
    std::mutex m_threads;
    std::mutex m_threadEnd;
    std::mutex m_sessionsClient;
    std::atomic<bool> b_servWorking = true;

    std::map<std::thread::id, std::map<unsigned int, unsigned int>> m_sessions;

    bool Init();

    bool setStartSocketSettings();

    void initAcceptThread();

    void acceptFunction();

    void freeFunction();

    void sendToAllFunction();

    void createThreadForClient(SOCKET clientSocket, SOCKADDR_IN clientSockIn);
public:
    Server(u_short port);
    void joinMainThread();
    ~Server();
};
