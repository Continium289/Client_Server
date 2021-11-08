#include <stdio.h>
#include <iostream>
#include "Server.h"
#include <thread>
#include "messageParser.h"

#pragma comment(lib, "Ws2_32.lib")

std::atomic<int> startThreadID = -1;

int getThreadID(std::thread& th) {
    std::thread::id nativeId = th.get_id();
    int* id = reinterpret_cast<int*>(&nativeId);

    return *id;
}

std::thread::id getThreadType(int threadID) {
    std::thread::id* tmp;
    tmp = reinterpret_cast<std::thread::id*>(&threadID);
    return *tmp;
}

bool Server::Init() {
    WSADATA wsaData;

    // Initialize Winsock
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    return iResult == 0;
}

bool Server::setStartSocketSettings() {
    s_listening = socket(AF_INET, SOCK_STREAM, 0);
    if (s_listening == INVALID_SOCKET)
    {
        return false;
    }

    sockaddr_in hint;
    hint.sin_family = AF_INET;
    hint.sin_port = htons(u_port);
    hint.sin_addr.S_un.S_addr = INADDR_ANY;

    bind(s_listening, (sockaddr*)&hint, sizeof(hint));

    //Tell Winsock the socket is for listening 
    listen(s_listening, SOMAXCONN);
    return true;
}

void Server::initAcceptThread() {
    t_Accept = std::thread([this]() {acceptFunction(); });
    t_CheckActual = std::thread([this]() {freeFunction(); });
    m_threadEnd.lock();
    t_SendToAll = std::thread([this]() { sendToAllFunction(); });
    startThreadID.store(getThreadID(t_SendToAll));
    m_threadEnd.unlock();
}

void Server::sendToAllFunction() {
    MSG msg;
    PeekMessage(&msg, 0, 0, 0, PM_REMOVE);
    while (b_servWorking.load() && GetMessage(&msg, 0, 0, 0) > 0) {
        if (msg.wParam == -1) {
            break;
        }

        m_sessionsClient.lock();

        msgParser ansParser((char*)msg.lParam, m_sessions, getThreadType(msg.wParam));
        const std::string& ans = ansParser.result;
        m_sessionsClient.unlock();

        m_main.lock();
        for (auto& it : m_sockets) {
            send(it.second, ans.c_str(), ans.size(), NULL);
        }
        delete[](char*)msg.lParam;
        m_main.unlock();
    }
}

void Server::createThreadForClient(SOCKET clientSocket, SOCKADDR_IN clientSockIn) {
    m_threads.lock();
    v_threads.push_back(std::thread([this, clientSocket, clientSockIn]() {
        char buffer[1000];
        //std::cout << "CREATED" << std::endl;
        m_main.lock();
        m_sockets.insert(std::make_pair(std::this_thread::get_id(), clientSocket));
        m_main.unlock();
        while (b_servWorking.load())
        {
            int k = recv(clientSocket, buffer, sizeof(buffer), NULL);
            if (k <= 0) {
                //std::cout << "CLIENT DISCONNECTED " << std::endl;
                m_main.lock();
                v_threadsNumsEnd.push_back(std::this_thread::get_id());
                m_main.unlock();
                break;
            }
            else if (k > 0) {
                char* ans = new char[k + 1];
                std::memcpy(ans, buffer, k);
                ans[k] = '\0';
                //std::cout << "Get " << ans << " length:" << k << std::endl;
                m_threadEnd.lock();
                PostThreadMessage(getThreadID(t_SendToAll), WM_APP, GetCurrentThreadId(), (LPARAM)ans);
                m_threadEnd.unlock();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
        }
        }));
    m_threads.unlock();
}

void Server::freeFunction() {
    while (b_servWorking.load()) {
        m_main.lock();
        m_threads.lock();
        if (!v_threadsNumsEnd.empty()) {
            for (auto itNum = v_threadsNumsEnd.begin(); itNum != v_threadsNumsEnd.end(); itNum++) {
                for (auto itThread = v_threads.begin(); itThread != v_threads.end(); itThread++) {
                    if (itThread->get_id() == *itNum) {
                        itThread->join();
                        v_threads.erase(itThread);
                        //std::cout << "Удалили поток с номером = " << *itNum << std::endl;

                        m_sessionsClient.lock();
                        auto itSessions = m_sessions.find(*itNum);
                        if (itSessions != m_sessions.end()) {
                            m_sessions.erase(itSessions);
                        }
                        m_sessionsClient.unlock();

                        auto itMap = m_sockets.find(*itNum);
                        closesocket(itMap->second);
                        m_sockets.erase(itMap);
                        v_threadsNumsEnd.erase(itNum);
                        break;
                    }
                }
                break;
            }
        }
        m_threads.unlock();
        m_main.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

}

void Server::acceptFunction() {
    while (b_servWorking.load()) {
        SOCKET client_socket;
        SOCKADDR_IN client_addr;
        int addrlen = sizeof(client_addr);

        if ((client_socket = accept(s_listening.load(), (struct sockaddr*)&client_addr, &addrlen)) != 0 && b_servWorking.load()) {
            //std::cout << "ACCEPT\n";
            createThreadForClient(client_socket, client_addr);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }

}

void Server::joinMainThread() {
    if (!t_SendToAll.joinable()) {
        return;
    }

    t_SendToAll.join();
}

void signalHandler(int type) {
    if (startThreadID.load() == -1) {
        std::signal(type, signalHandler);
        return;
    }

    PostThreadMessage(startThreadID.load(), WM_APP, -1, (LPARAM)0);
}

Server::Server(u_short port = 100) : u_port(port) {
    if (!Init()) {
        throw std::exception("Не удалось создать сервер!");
    }

    if (!setStartSocketSettings()) {
        throw std::exception("Не удалось создать сокет!");
    }

    std::signal(SIGINT, signalHandler);
    std::signal(SIGBREAK, signalHandler);

    initAcceptThread();

}

Server::~Server() {
    b_servWorking.store(false);
    closesocket(s_listening.load());

    t_Accept.join();

    m_main.lock();
    for (auto& it : m_sockets) {
        closesocket(it.second);
    }
    m_main.unlock();

    t_CheckActual.join();

    m_threads.lock();
    for (auto& it : v_threads) {
        it.join();
    }
    m_threads.unlock();

    std::cout << "Good end!\n";
    startThreadID.store(-1);
}

int main()
{
    setlocale(LC_ALL, "Russian");
    Server t(54320);
    t.joinMainThread();
}