#include <iostream>
#include <string>
#include "Client.h"
#include <sstream>

bool Client::Init() {
    WSADATA wsaData;

    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    return iResult == 0;
}

bool Client::setStartSocketSettings() {
    if ((s_server = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        return false;
    }

    SOCKADDR_IN addr;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(u_port); //порт
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr.s_addr);
    if (connect(s_server, (SOCKADDR*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        std::cout << "Не удалось подключиться к серверу\n";
        return false;
    }

    return true;
}

void Client::Send(std::string& message) {
    if (send(s_server, message.c_str(), message.length(), 0) == SOCKET_ERROR) {
        //std::cout << "send failed with error: " << WSAGetLastError() << std::endl;
    }
    else {
        //std::cout << "Отправили: " << message << std::endl;
    }
}

void Client::createThreadForRecv() {
    t_Recv = std::thread([this]() {recvFunction(); });
}

void Client::recvFunction() {
    char buffer[1000];
    memset(buffer, 0, sizeof(buffer));
    while (b_Work) {
        int res = recv(s_server, buffer, sizeof(buffer), 0);
        std::cout << res << std::endl;
        if (res > 0) {
            std::cout << "Получили следующее сообщение:\n " << buffer << std::endl;
        }
        else {
            std::cout << "Сервер закрыл соединение\n";
            closesocket(s_server);
            b_Work = false;
            break;
        }

        memset(buffer, 0, sizeof(buffer));
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

Client::Client(u_short port = 100) : u_port(port), b_Work(true) {
    if (!Init()) {
        throw std::exception("Не получилось подключиться");
    }

    if (!setStartSocketSettings()) {
        throw std::exception("Возникли проблемы с сокетом");
    }

    createThreadForRecv();

    bufferSend = new char[1000];
    memset(bufferSend, 0, sizeof(bufferSend));
}

Client::~Client() {
    closesocket(s_server);
    b_Work = false;
    t_Recv.join();
    delete[] bufferSend;
}

int main()
{
    std::string forSend;
    setlocale(LC_ALL, "Russian");

    try {
        Client cl(54320);

        while (std::getline(std::cin, forSend) && cl.b_Work)
        {
            cl.Send(forSend);
        }
    }
    catch (std::exception exp) {
        std::cout << exp.what() << std::endl;
        return 0;
    }
}

