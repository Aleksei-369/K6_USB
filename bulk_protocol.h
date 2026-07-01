#ifndef __BULK_PROTOCOL_H
#define __BULK_PROTOCOL_H

#include <stdint.h>

#pragma pack(push, 1) // Запрещаем компилятору раздувать структуру в ОЗУ (строго байт-в-байт)

// 1. Структура для коротких команд управления (Звук, сброс и т.д.)
typedef struct {
    uint8_t  command_id;      // 1 = Звуковой зуммер, 2 = Сброс АЦП, 3 = Калибровка
    uint8_t  param1;          // Параметр команды (1 - сингл писк, 2 - дабл писк)
    uint8_t  param2;          // Резерв
    uint8_t  reserved[60];    // Добивка структуры до фиксированного размера
} CmdPacket_t;

// 2. Структура для кусков формы сигнала возбуждения ЦАП
typedef struct {
    uint16_t chunk_index;     // Порядковый номер куска (0, 1, 2...701)
    uint8_t  payload_len;     // Сколько чистых байт данных внутри пакета (макс 60)
    uint8_t  data[60];        // Кусочек массива формы сигнала
} WaveformPacket_t;

// 3. ГЛАВНЫЙ ЕДИНЫЙ КОНТЕЙНЕР (Универсальный 64-байтовый USB Bulk фрейм)
typedef struct {
    uint8_t  packet_type;     // МАРКЕР ТИПА ПАКЕТА: 1 = Команда, 2 = Форма сигнала ЦАП
    
    union {
        CmdPacket_t      cmd; // Если packet_type == 1, память интерпретируется как команда
        WaveformPacket_t wf;  // Если packet_type == 2, память интерпретируется как кусок сигнала
        uint8_t          raw[63]; // Сырой массив для универсальной передачи по USB-шине
    } payload;
} USB_BulkFrame_t;

#pragma pack(pop)

#endif /* __BULK_PROTOCOL_H */