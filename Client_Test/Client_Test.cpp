#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <iostream>
#include <fstream>
#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <Windows.h>

#pragma comment(lib, "ws2_32.lib")

#define MESSAGE_SIZE 128   // Размер передаваемых сообщений

void send_active(SOCKET sock)
{
    char active[] = "Client is active";
    while(1)
    {
        send(sock, active, sizeof(active), 0);
        Sleep(1000);
    }
}

// Функция создания и отправки скриншота рабочего стола
void screenshot_send(SOCKET sock)
{
    HDC hdc         = GetDC(NULL);
    int width       = GetSystemMetrics(SM_CXSCREEN);
    int height      = GetSystemMetrics(SM_CYSCREEN);
    HDC hdcMem      = CreateCompatibleDC(hdc);
    HBITMAP hBitmap = CreateCompatibleBitmap(hdc, width, height);
    SelectObject(hdcMem, hBitmap);
    BitBlt(hdcMem, 0, 0, width, height, hdc, 0, 0, SRCCOPY);
    ReleaseDC(NULL, hdc);

    char msg[] = "Screenshot";
    char* screenshot_data = new char[width * height * 4]; // 4 байта на пиксель (RGB + alpha)
    GetBitmapBits(hBitmap, width * height * 4, screenshot_data);

    // Отправка сообщения о передаче скриншота
    send(sock, msg, sizeof(msg), 0);
    // Отправка ширины рабочего стола
    send(sock, (char*)&width, sizeof(int), 0);
    // Отправка высоты рабочего стола
    send(sock, (char*)&height, sizeof(int), 0);
    // Отправка скриншота на сервер
    send(sock, screenshot_data, width * height * 4, 0);

    DeleteObject(hBitmap);
    DeleteDC(hdcMem);
}

// Функция приема и обработки полученного скриншота от сервера
int handle_message_from_server(SOCKET sock, char* message)
{
    // Пустое сообщение
    if (strcmp(message, "") == 0)
    {
        return 0;
    }

    // Запрос скриншота рабочего стола
    if (strcmp(message, "screenshot") == 0)
    {
        screenshot_send(sock);
        return 0;
    }

    // Отключение сервера
    if (strcmp(message, "server_off") == 0)
    {
        // Закрытие сокета
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    std::cout << "Non-standart message from server:\n" << message << std::endl;
    return 0;
}

// Функция получения сообщений от сервера
void handle_server(SOCKET sock)
{
    while(1)
    {
        int res;
        int bytes_received;
        char message[MESSAGE_SIZE];
        
        bytes_received = recv(sock, message, MESSAGE_SIZE, 0);
        if (bytes_received <= 0)
        {
            std::cout << "recv() is fail! bytes_received = [" << bytes_received << "] while [>= 0] is expected\n";
            closesocket(sock);
            break;
        }

        message[bytes_received] = '\0';
        res = handle_message_from_server(sock, message);
        if (res == 1)
        {
            break;
        }
    }
}

int main()
{
    // Инициализация сокета
    WSADATA wsaData;
    WORD ver = MAKEWORD(2, 2);
    int wsResult = WSAStartup(ver, &wsaData);
    if (wsResult != 0)
    {
        std::cout << "WSAStartup() si fail! error = [" << wsResult << "]\n";
        return 1;
    }

    // Создание сокета
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET)
    {
        std::cout << "socket() is fail!  error = [" << WSAGetLastError() << "]\n";
        WSACleanup();
        return 1;
    }

    // Установка соединения с сервером
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(1111); // порт сервера
    inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr); // адрес сервера
    connect(sock, (SOCKADDR*)&serverAddr, sizeof(serverAddr));

    // Отправка имени компьютера
    char computer[] = "computer";
    send(sock, computer, strlen(computer), 0);

    Sleep(1000);
    // Отправка имени клиента
    char user[] = "user";
    send(sock, user, strlen(user), 0);

    // Поток, периодически посылающий серверу сообщение об активности клиента
    CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)send_active, (LPVOID)sock, NULL, NULL);

    // Обработчик сообщений от сервера
    handle_server(sock);

    return 0;
}