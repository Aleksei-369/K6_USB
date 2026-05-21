// mbuild USB_UDP_Bridge.c -I. -L. -llibusb-1.0.lib

#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "libusb.h"

// Подключаем системные библиотеки Windows для сети и USB
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "libusb-1.0.lib")

#define VID 0x0483
#define PID 0x5750
#define PORT_TO_MATLAB   25000 
#define PORT_FROM_MATLAB 25002 
#define SIZE_IN   10064        
#define SIZE_OUT  40000        

libusb_device_handle *handle = NULL;
libusb_context *ctx = NULL;
SOCKET sock = INVALID_SOCKET;

void cleanup() {
    printf("\n[Bridge] Cleaning up and exiting...\n");
    if (handle) {
        libusb_release_interface(handle, 0);
        libusb_close(handle);
    }
    if (ctx) libusb_exit(ctx);
    if (sock != INVALID_SOCKET) closesocket(sock);
    WSACleanup();
}

int main() {
    // 1. ПРИОРИТЕТЫ (для защиты от лагов Windows 11)
    SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS);
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    SetThreadAffinityMask(GetCurrentThread(), 0x04); // Ядро №2 (P-core)
    SetProcessPriorityBoost(GetCurrentProcess(), TRUE);

    // 2. СЕТЬ (UDP)
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("Winsock Init Failed.\n");
        return -1;
    }
    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    struct sockaddr_in addr_to_matlab;
    addr_to_matlab.sin_family = AF_INET;
    addr_to_matlab.sin_port = htons(PORT_TO_MATLAB);
    addr_to_matlab.sin_addr.s_addr = inet_addr("127.0.0.1");

    struct sockaddr_in local_addr;
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(PORT_FROM_MATLAB);
    local_addr.sin_addr.s_addr = INADDR_ANY;
    bind(sock, (struct sockaddr *)&local_addr, sizeof(local_addr));

    // Неблокирующий режим для приема команд
    u_long mode = 1;
    ioctlsocket(sock, FIONBIO, &mode);

    // 3. USB ИНИЦИАЛИЗАЦИЯ
    if (libusb_init(&ctx) < 0) return -1;
    handle = libusb_open_device_with_vid_pid(ctx, VID, PID);
    if (!handle) {
        printf("\n!!! ERROR: STM32 NOT FOUND !!!\nCheck USB cable and VID/PID.\n");
        cleanup();
        printf("Press Enter to exit...");
        getchar();
        return -1; 
    }
    
    libusb_claim_interface(handle, 0);
    printf("========================================\n");
    printf("      USB <-> UDP BRIDGE STARTED        \n");
    printf("========================================\n");
    printf("Streaming: USB -> UDP:%d\n", PORT_TO_MATLAB);
    printf("Listening: UDP:%d -> USB\n", PORT_FROM_MATLAB);

    uint8_t usb_in_buffer[SIZE_IN];
    uint8_t udp_in_buffer[SIZE_OUT];

    // 4. ГЛАВНЫЙ ЦИКЛ
    while (1) {
        int transferred = 0;
        // Читаем из USB (таймаут 35мс, чтобы успевать в 50Гц)
        int r = libusb_bulk_transfer(handle, 0x81, usb_in_buffer, SIZE_IN, &transferred, 35);
        
        if (r == 0) {
            // Шлем в Simulink
            sendto(sock, (const char*)usb_in_buffer, SIZE_IN, 0, (struct sockaddr*)&addr_to_matlab, sizeof(addr_to_matlab));
        } else if (r != LIBUSB_ERROR_TIMEOUT) {
            printf("\nUSB FATAL ERROR: %s\n", libusb_error_name(r));
            break; 
        }

        // Проверяем, не прислал ли Simulink команду (40кб)
        int received_bytes = recv(sock, (char*)udp_in_buffer, SIZE_OUT, 0);
        if (received_bytes == 40000) {
                printf("[CMD] Sending 40000 bytes to STM32...\n");
                // Отправляем данные
                libusb_bulk_transfer(handle, 0x01, udp_in_buffer, 40000, &transferred, 500);
        }
        
        // Даем Windows немного подышать (1мс)
        Sleep(1);
    }

    cleanup();

    printf("\n--- Press Enter to close this window ---");
    getchar(); // Ждет нажатия клавиши

    return 0;
}