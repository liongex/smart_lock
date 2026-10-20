#ifndef DRIVER_RFID_H
#define DRIVER_RFID_H

#include <stdint.h>

// Tipo da função que o RTOS Manager vai injetar aqui
typedef void (*rfid_scanned_cb_t)(uint64_t uid);

void rfid_hw_init(rfid_scanned_cb_t callback);
void rfid_hw_pause(void);
void rfid_hw_resume(void);

#endif // DRIVER_RFID_H