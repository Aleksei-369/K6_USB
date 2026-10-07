#include "mex.h"
#include <winsock2.h>

#pragma comment(lib, "ws2_32.lib")

void mexFunction(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[]) 
{
    // 🌟 ВАШ ТОЧНЫЙ ИСПРАВЛЕННЫЙ ЗАМОК: Проверяем строго первый аргумент prhs[0]!
    if (nrhs != 1 || !mxIsDouble(prhs[0]) || mxIsComplex(prhs[0])) {
        mexErrMsgIdAndTxt("K6_Network:InvalidInput", "На вход должен подаваться вещественный массив double!");
    }

    // Извлекаем указатель на данные из первого элемента prhs[0]
    double *dataVector = mxGetPr(prhs[0]);
    int totalElements = (int)mxGetNumberOfElements(prhs[0]);
    int totalBytes = totalElements * sizeof(double);

    // Инициализация Winsock ядра Windows
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        mexErrMsgIdAndTxt("K6_Network:WSAError", "Ошибка инициализации Winsock!");
    }

    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        WSACleanup();
        mexErrMsgIdAndTxt("K6_Network:SocketError", "Не удалось создать системный UDP-сокет!");
    }

    // ТИТАНОВЫЙ ФИКС ДЕДЛОКА: Переводим сокет в НЕБЛОКИРУЮЩИЙ режим (Non-blocking)
    u_long mode = 1; // 1 = включить non-blocking
    ioctlsocket(sock, FIONBIO, &mode);

    struct sockaddr_in srvAddr;
    srvAddr.sin_family = AF_INET;
    srvAddr.sin_port = htons(25010); // Наш целевой порт UDP Receive
    srvAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    // Мгновенный выстрел монолита в ОЗУ Windows без ожидания ответа Симулинка
    sendto(sock, (const char*)dataVector, totalBytes, 0, (struct sockaddr*)&srvAddr, sizeof(srvAddr));

    closesocket(sock);
    WSACleanup();
}