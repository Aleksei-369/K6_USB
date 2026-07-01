// Компиляция: mbuild USB_UDP_Bridge_v2.0.c -output USB_UDP_Bridge_v2.0 -I. -L. -llibusb-1.0.lib

#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "libusb.h"
#include "bulk_protocol.h" // Наш вечный промышленный паспорт протокола

// Подключаем системные библиотеки Windows для работы с сетью и USB
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "libusb-1.0.lib")

#define VID 0x0483
#define PID 0x5750

// Разводим потоки данных на ПК по независимым портам
#define PORT_TO_MATLAB     25000 // Выход в ПК: Поток сигналов датчиков (10064 байта)
#define PORT_WAVEFORM_FROM 25002 // Вход из ПК: Форма сигнала возбуждения ЦАП (40000 байт)
#define PORT_COMMANDS_FROM 25003 // Вход из ПК: Мгновенные команды звука/управления (64 байта)

#define SIZE_IN            10064        
#define SIZE_OUT           40000        

libusb_device_handle *handle = NULL;
libusb_context *ctx = NULL;

SOCKET sock_main = INVALID_SOCKET; // Базовый сокет (Отправка сигналов в ПК + Прием 40кб)
SOCKET sock_cmd  = INVALID_SOCKET; // Выделенный независимый сокет только под команды звука

void cleanup() {
    printf("\n[Bridge] Cleaning up resources and exiting...\n");
    if (handle) {
        libusb_release_interface(handle, 0);
        libusb_close(handle);
    }
    if (ctx) libusb_exit(ctx);
    if (sock_main != INVALID_SOCKET) closesocket(sock_main);
    if (sock_cmd  != INVALID_SOCKET) closesocket(sock_cmd);
    WSACleanup();
}

int main() {
    // 1. ВЫСШИЙ ПРИОРИТЕТ РЕАЛЬНОГО ВРЕМЕНИ (Защита от энергосберегающих лагов Windows 11)
    SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS);
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    SetThreadAffinityMask(GetCurrentThread(), 0x04); // Жесткая привязка к Ядру №2 (Производительное P-core)
    SetProcessPriorityBoost(GetCurrentProcess(), TRUE);

    // 2. ИНИЦИАЛИЗАЦИЯ СЕТЕВОЙ ПОДСИСТЕМЫ WINDOWS (WINSOCK)
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("[ERROR] Winsock Initialization Failed.\n");
        return -1;
    }

    // Создаем и привязываем сокет Потока А и В (Порт 25002)
    sock_main = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    struct sockaddr_in addr_to_matlab;
    addr_to_matlab.sin_family = AF_INET;
    addr_to_matlab.sin_port = htons(PORT_TO_MATLAB);
    addr_to_matlab.sin_addr.s_addr = inet_addr("127.0.0.1");

    struct sockaddr_in wf_addr;
    wf_addr.sin_family = AF_INET;
    wf_addr.sin_port = htons(PORT_WAVEFORM_FROM);
    wf_addr.sin_addr.s_addr = INADDR_ANY; // Слушать со всех сетевых интерфейсов
    if (bind(sock_main, (struct sockaddr *)&wf_addr, sizeof(wf_addr)) == SOCKET_ERROR) {
        printf("[ERROR] Failed to bind Main Socket to port %d\n", PORT_WAVEFORM_FROM);
        cleanup(); return -1;
    }

    // Создаем и привязываем ОТДЕЛЬНЫЙ сокет Потока Б для команд звука (Порт 25003)
    sock_cmd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    struct sockaddr_in cmd_addr;
    cmd_addr.sin_family = AF_INET;
    cmd_addr.sin_port = htons(PORT_COMMANDS_FROM);
    cmd_addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(sock_cmd, (struct sockaddr *)&cmd_addr, sizeof(cmd_addr)) == SOCKET_ERROR) {
        printf("[ERROR] Failed to bind Command Socket to port %d\n", PORT_COMMANDS_FROM);
        cleanup(); return -1;
    }

    // Переводим ОБА сокета в неблокирующий режим (FIONBIO)
    // Благодаря этому recv() выполняется за наносекунды и не тормозит цикл, если данных нет
    u_long mode = 1;
    ioctlsocket(sock_main, FIONBIO, &mode);
    ioctlsocket(sock_cmd,  FIONBIO, &mode);

    // 3. ИНИЦИАЛИЗАЦИЯ ДРАЙВЕРА USB (LIBUSB)
    if (libusb_init(&ctx) < 0) return -1;
    handle = libusb_open_device_with_vid_pid(ctx, VID, PID);
    if (!handle) {
        printf("\n!!! ERROR: STM32 DEVICE NOT FOUND !!!\nCheck Bulk firmware, VID/PID, and USB cable.\n");
        cleanup();
        printf("Press Enter to exit...");
        getchar();
        return -1; 
    }
    
    if (libusb_claim_interface(handle, 0) < 0) {
        printf("[ERROR] Cannot claim USB interface 0.\n");
        cleanup(); return -1;
    }

    printf("==================================================\n");
    printf("     PORT-SEPARATED UNIFIED USB <-> UDP BRIDGE    \n");
    printf("==================================================\n");
    printf("Streaming (Upstream): USB -> UDP:127.0.0.1:%d\n", PORT_TO_MATLAB);
    printf("Waveform  channel (Downstream): listening on UDP:%d\n", PORT_WAVEFORM_FROM);
    printf("Command   channel (Downstream): listening on UDP:%d\n", PORT_COMMANDS_FROM);
    printf("Status: BUS SECURE MUTEX ACTIVATED.\n");
    printf("==================================================\n\n");

    uint8_t usb_in_buffer[SIZE_IN];
    uint8_t udp_wf_buffer[SIZE_OUT];
    uint8_t udp_cmd_buffer[64]; // Буфер под 64 байта из Симулинка
    
    USB_BulkFrame_t tx_frame; // Наш универсальный структурный контейнер на отправку в USB
    int transferred = 0;
    
    // Железный семафор: блокирует командный канал на 45 мс, пока льются 40 000 байт формы ЦАП
    bool is_usb_busy = false; 

    // 4. ГЛАВНЫЙ ИНТЕЛЛЕКТУАЛЬНЫЙ ЦИКЛ ОБМЕНА ДАННЫМИ
    while (1) {
        transferred = 0;
        
        // -------------------------------------------------------------------------
        // ПОТОК А (ВВЕРХ В ПК): Сигнал датчиков из STM32 -> транзитом в Simulink
        // Теймаут 35мс, чтобы гарантированно успевать в измерительную сетку 50 Гц (20 мс)
        // -------------------------------------------------------------------------
        int r = libusb_bulk_transfer(handle, 0x81, usb_in_buffer, SIZE_IN, &transferred, 35);
        if (r == 0 && transferred == SIZE_IN) {
            sendto(sock_main, (const char*)usb_in_buffer, SIZE_IN, 0, (struct sockaddr*)&addr_to_matlab, sizeof(addr_to_matlab));
        } else if (r != LIBUSB_ERROR_TIMEOUT) {
            printf("\n[FATAL] USB Hardware Error: %s\n", libusb_error_name(r));
            break; 
        }

        // -------------------------------------------------------------------------
        // ПОТОК Б (ВНИЗ В ПЛАТУ): Проверяем сокет КАНАЛА КОМАНД ЗВУКА (Порт 25003)
        // Опрашивается ТОЛЬКО если шина USB свободна от передачи тяжелой формы!
        // -------------------------------------------------------------------------
        if (!is_usb_busy) {
            int cmd_bytes = recv(sock_cmd, (char*)udp_cmd_buffer, 64, 0);
            if (cmd_bytes == 64) {
                is_usb_busy = true; // Захватываем мьютекс шины USB
                
                // Наполняем контейнер по строгим логическим именам структур паспорта протокола
                memset(&tx_frame, 0, sizeof(USB_BulkFrame_t));
                tx_frame.packet_type = 1;                             // 1 = ТИП КОМАНДЫ
                tx_frame.payload.cmd.command_id = 1;                  // 1 = Звуковой зуммер
                tx_frame.payload.cmd.param1 = udp_cmd_buffer[0];      // Код писка (1-сингл, 2-дабл)
                
                printf("[BEEP] Bus Free. Forwarding structural command (Code: %d) to USB...\n", tx_frame.payload.cmd.param1);
                
                // Выстреливаем монолитные 64 байта фрейма в STM32 (таймаут минимальный - 10 мс)
                libusb_bulk_transfer(handle, 0x01, (uint8_t*)&tx_frame, 64, &transferred, 10);
                
                is_usb_busy = false; // Освобождаем мьютекс шины USB
            }
        }

        // -------------------------------------------------------------------------
        // ПОТОК В (ВНИЗ В ПЛАТУ): Проверяем сокет КАНАЛА ФОРМЫ ЦАП (Порт 25002)
        // -------------------------------------------------------------------------
        int wf_bytes = recv(sock_main, (char*)udp_wf_buffer, SIZE_OUT, 0);
        if (wf_bytes == 40000) {
            is_usb_busy = true; // НАМЕРТВО БЛОКИРУЕМ ШИНУ И КАНАЛ КОМАНД НА 45 МС!
            
            printf("[WAVEFORM] 40000 bytes arrived. Locking USB bus for structured slicing...\n");
            
            int bytes_left = 40000;
            int src_offset = 0;
            uint16_t packet_counter = 0;
            
            // Нарезаем 40 000 байт на 702 пакета, оставляя место под заголовки внутри структур
            while (bytes_left > 0) {
                // Под чистые данные внутри WaveformPacket_t выделено строго 57 байт
                int chunk_size = (bytes_left > 57) ? 57 : bytes_left;
                
                memset(&tx_frame, 0, sizeof(USB_BulkFrame_t));
                tx_frame.packet_type = 2;                             // 2 = ТИП ФОРМЫ ЦАП
                tx_frame.payload.wf.chunk_index = packet_counter;     // Порядковый номер куска
                tx_frame.payload.wf.payload_len = (uint8_t)chunk_size; // Сколько чистых байт внутри
                
                // Копируем кусок сигнала из MATLAB напрямую в массив данных структуры фрейма
                memcpy(tx_frame.payload.wf.data, &udp_wf_buffer[src_offset], chunk_size);
                
                // Отправляем монолитную 64-байтовую структуру в шину USB
                libusb_bulk_transfer(handle, 0x01, (uint8_t*)&tx_frame, 64, &transferred, 100);
                
                bytes_left -= chunk_size;
                src_offset += chunk_size;
                packet_counter++;
            }
            printf("[WAVEFORM] Transmission finished. Total structural chunks: %d. Unlocking bus.\n", packet_counter);
            
            is_usb_busy = false; // СНИМАЕМ БЛОКИРОВКУ, шина снова открыта для мгновенных команд!
        }
        
        // Даем Windows немного подышать (1мс), защищая процессор ноутбука от 100% загрузки
        Sleep(1);
    }

    cleanup();

    printf("\n--- Press Enter to close bridge ---");
    getchar();

    return 0;
}