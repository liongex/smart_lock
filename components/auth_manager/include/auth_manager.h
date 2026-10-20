#ifndef AUTH_MANAGER_H
#define AUTH_MANAGER_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// Recebe os handles das filas lá do rtos_manager e inicia a tarefa
void auth_manager_init(QueueHandle_t rfid_queue, QueueHandle_t keypad_queue);

#endif // AUTH_MANAGER_H