#include "driver_pir.h"
#include "driver/gpio.h"

void pir_hw_init(gpio_num_t pir_pin)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << pir_pin),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE, // CORREÇÃO: Pino flutuante para evitar o divisor de tensão
        .intr_type = GPIO_INTR_DISABLE        
    };
    gpio_config(&io_conf);
}