#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <iostream>
#include <fstream>
#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <map>
#include <string>

#pragma comment(lib, "ws2_32.lib")

#define MESSAGE_SIZE 128   // Размер передаваемых сообщений
#define MAX_CONNECT 128    // Количество поддерживаемых подключений

// Список подключенных клиентов
std::map<SOCKET, std::map<std::string, std::string>> clients;

// Сокет сервера
SOCKET listening_socket;

// Функция приема и обработки полученного скриншота от клиента
void screenshot_conversion(SOCKET client_socket)
{
    // Получение размера экрана
    int screen_width;
    int screen_height;

    recv(client_socket, (char*)&screen_width,  sizeof(int), 0);
    recv(client_socket, (char*)&screen_height, sizeof(int), 0);

    // Получение скриншота
    char* screenshot_data = new char[screen_width * screen_height * 4];   // 4 байта на пиксель (RGB + alpha)
    recv(client_socket, screenshot_data, screen_width * screen_height * 4, 0);

    // Получение текущего времени
    time_t now = time(0);
    char* dt   = ctime(&now);

    // Сохранение скриншота в файл
    char filename[256];
    sprintf(filename, "screenshot_%s_%s.bmp", clients[client_socket]["user"], dt);

    // Открыть файл для вывода
    std::ofstream file(filename, std::ios::binary);

    // Запись BMP header
    unsigned short int bfType      = 0x4d42; // 'BM'
    unsigned int bfSize            = 54 + screen_width * screen_height * 3;
    unsigned short int bfReserved1 = 0;
    unsigned short int bfReserved2 = 0;
    unsigned int bfOffBits         = 54;

    file.write( (char*)&bfType,      sizeof(unsigned short int) );
    file.write( (char*)&bfSize,      sizeof(unsigned int)       );
    file.write( (char*)&bfReserved1, sizeof(unsigned short int) );
    file.write( (char*)&bfReserved2, sizeof(unsigned short int) );
    file.write( (char*)&bfOffBits,   sizeof(unsigned int)       );

    // Запись DIB header
    unsigned int biSize           = 40;
    unsigned int biWidth          = screen_width;
    unsigned int biHeight         = screen_height;
    unsigned short int biPlanes   = 1;
    unsigned short int biBitCount = 24;
    unsigned int biCompression    = 0;
    unsigned int biSizeImage      = screen_width * screen_height * 3;
    unsigned int biXPelsPerMeter  = 0;
    unsigned int biYPelsPerMeter  = 0;
    unsigned int biClrUsed        = 0;
    unsigned int biClrImportant   = 0;

    file.write( (char*)&biSize,          sizeof(unsigned int)       );
    file.write( (char*)&biWidth,         sizeof(unsigned int)       );
    file.write( (char*)&biHeight,        sizeof(unsigned int)       );
    file.write( (char*)&biPlanes,        sizeof(unsigned short int) );
    file.write( (char*)&biBitCount,      sizeof(unsigned short int) );
    file.write( (char*)&biCompression,   sizeof(unsigned int)       );
    file.write( (char*)&biSizeImage,     sizeof(unsigned int)       );
    file.write( (char*)&biXPelsPerMeter, sizeof(unsigned int)       );
    file.write( (char*)&biYPelsPerMeter, sizeof(unsigned int)       );
    file.write( (char*)&biClrUsed,       sizeof(unsigned int)       );
    file.write( (char*)&biClrImportant,  sizeof(unsigned int)       );

    // Запись данных пикселей
    for (int y = 0; y < screen_height; y++)
    {
        for (int x = 0; x < screen_width; x++)
        {
            unsigned char r = screenshot_data[(y * screen_width * 4) + (x * 4) + 0];
            unsigned char g = screenshot_data[(y * screen_width * 4) + (x * 4) + 1];
            unsigned char b = screenshot_data[(y * screen_width * 4) + (x * 4) + 2];
            file << b << g << r;
        }
    }

    file.close();
}

// Функция обработки входящих сообщений от клиента
void handle_message_from_client(SOCKET client_socket, char* message)
{
    // Пустое сообщение
    if (strcmp(message, "") == 0)
    {
        return;
    }
    // Стартовое сообщение клиента
    if (strcmp(message, "Client started") == 0)
    {
        
        return;
    }

    // Сообщение клиентом о его активности
    if (strcmp(message, "Client is active") == 0)
    {
        // Обновление времени последней активности клиента
        time_t now = time(0);
        char* dt = ctime(&now);
        clients[client_socket]["last_activity"] = dt;
        return;
    }

    // Посыл скриншота клиентом
    if ((strcmp(message, "Screenshot") == 0))
    {
        screenshot_conversion(client_socket);
        return;
    }

    std::cout << "Non-standard message from client:\n" << message << std::endl;
}

void handle_client(SOCKET client_socket)
{
    while (1)
    {
        char message[MESSAGE_SIZE];
        int bytes_received;
        bytes_received = recv(client_socket, message, MESSAGE_SIZE, 0);
        if (bytes_received <= 0)
        {
            std::cout << "recv() is fail! bytes_received = [" << bytes_received << "] while [>= 0] is expected\n";
            break;
        }
        message[bytes_received] = '\0';
        handle_message_from_client(client_socket, message);
    }
    closesocket(client_socket);
}

void handle_message_from_operator(std::string message)
{
    // Пустое сообщение
    if (message.compare("") == 0)
    {
        return;
    }

    // Запрос подключенных клиентов
    if (message.compare("clients_list") == 0)
    {
        int num = 0;
        for (auto& item_out : clients)
        {
            num++;
            std::cout << "Client #" << num << std::endl;
            for (auto& item_in : item_out.second)
            {
                std::cout << "    " << item_in.first << " = " << item_in.second;
                if (item_in.first != "last_activity")
                {
                    std::cout << std::endl;
                }
            }
            std::cout << std::endl;
        }
        return;
    }

    // Запрос последнего активного времени конкретного клиента
    if (message.find("last_time") == 0)
    {
        for (auto& item_out : clients)
        {
            std::cout << message << std::endl;
            for (auto& item_in : item_out.second)
            {
                std::cout << message << item_in.second << std::endl;
                if (message.find(item_in.second) == 0)
                {
                    std::cout << message << item_in.second << std::endl;
                    std::cout << "Last active time client's #" << item_in.second << " = [" << clients[item_out.first]["last_activity"] << "]\n";
                    return;
                }
            }
        }

        std::cout << "Client does not corrected!\n";
        return;
    }

    // Запрос скриншота рабочего стола клиента
    if (message.find("screenshot") == 0)
    {
        char msg[] = "screenshot";
        for (auto& item_out : clients)
        {
            for (auto& item_in : item_out.second)
            {
                if (message.find(item_in.second) == 0)
                {
                    send(item_out.first, msg, sizeof(msg), NULL);
                    return;
                }
            }
        }
        std::cout << "Client does not corrected!\n";
        return;
    }

    // Запрос на закрытие сервера
    if (message.compare("server_off") == 0)
    {
        char msg[] = "server_off";
        for (auto& item : clients)
        {
            send(item.first, msg, sizeof(msg), NULL);
            // Закрытие сокета с клиентом
            closesocket(item.first);
        }

        // Закрытие сокета прослушивания
        closesocket(listening_socket);
        WSACleanup();
        return;
    }

    // Другие любые строки
    std::cout << "Incorrect command!\n";

}

// Функция обработки команд от оператора
void handle_operator()
{
    std::string message;
    while (1)
    {
        std::cin >> message;
        handle_message_from_operator(message);
        if (message.compare("server_off") == 0)
        {
            break;
        }
    }
}

int main()
{
    // Инициализация WinSock
    WSADATA wsaData;
    WORD ver = MAKEWORD(2, 1);
    int wsResult = WSAStartup(ver, &wsaData);
    if (wsResult != 0)
    {
        std::cout << "WSAStartup() si fail! error = [" << wsResult << "]\n";
        return 1;
    }

    // Создание сокета
    listening_socket = socket(AF_INET, SOCK_STREAM, NULL);
    if (listening_socket == INVALID_SOCKET)
    {
        std::cout << "socket() is fail!  error = [" << WSAGetLastError() << "]\n";
        WSACleanup();
        return 1;
    }

    // Связывание сокета с адрессом и портом
    SOCKADDR_IN server_address;
    int sizeof_server_addr = sizeof(server_address);
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(1111);
    server_address.sin_addr.s_addr = inet_addr("127.0.0.1");//INADDR_ANY;
    if (bind(listening_socket, (SOCKADDR*)&server_address, sizeof_server_addr) == SOCKET_ERROR)
    {
        std::cout << "bind() is fail! error = [" << WSAGetLastError() << "]\n";
        closesocket(listening_socket);
        WSACleanup();
        return 1;
    }

    // Прослушивание входящих подключений
    if (listen(listening_socket, SOMAXCONN) == SOCKET_ERROR)
    {
        std::cout << "listen() is fail! error = [ " << WSAGetLastError() << "]\n";
        closesocket(listening_socket);
        WSACleanup();
        return 1;
    }

    // Обработчик команд сервера
    CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)handle_operator, NULL, NULL, NULL);

    // Прием входящих подключений
    SOCKET client_socket;
    for (int i = 0; i < MAX_CONNECT; i++)
    {
        client_socket = accept(listening_socket, (SOCKADDR*)&server_address, &sizeof_server_addr);
        if (client_socket == INVALID_SOCKET)
        {
            std::cout << "accept() is fail! error = [" << WSAGetLastError() << "]\n";
            closesocket(listening_socket);
            WSACleanup();
            return 1;
        }

        char computer[MESSAGE_SIZE];
        char user[MESSAGE_SIZE];
        int bytes_received;

        // Получение имени компьютера
        bytes_received = recv(client_socket, computer, MESSAGE_SIZE, 0);
        if (bytes_received <= 0)
        {
            std::cout << "recv(computer) is fail! bytes_received = [" << bytes_received << "] while [>= 0] is expected\n";
            break;
        }
        computer[bytes_received] = '\0';

        // Получение имени клиента
        bytes_received = recv(client_socket, user, MESSAGE_SIZE, 0);
        if (bytes_received <= 0)
        {
            std::cout << "recv(user) is fail! bytes_received = [" << bytes_received << "] while [>= 0] is expected\n";
            break;
        }
        user[bytes_received] = '\0';

        clients[client_socket] = {
                                   
        };
        // Запись домена и ip клиента
        clients[client_socket] = {
                                  { "domain",    "domain"                           },
                                  { "ip",        inet_ntoa(server_address.sin_addr) },
                                  { "computer",  computer                           },
                                  { "user",      user                               }
        };

        // Обработчик запросов клиента
        CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)handle_client, (LPVOID)client_socket, NULL, NULL);
    }

    return 0;
}