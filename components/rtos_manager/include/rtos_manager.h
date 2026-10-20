#ifndef RTOS_MANAGER_H
#define RTOS_MANAGER_H

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

// Definimos o Bit 0 para sinalizar se o sistema está acordado
#define BIT_SYSTEM_AWAKE (1 << 0)

// Deixa o handle exposto para que as outras tasks (teclado, rfid) possam ler
extern EventGroupHandle_t system_events;

void rtos_system_init(void);

#endif // RTOS_MANAGER_H