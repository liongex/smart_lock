#include "driver_keypad.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// TODO: Substitua pelos pinos reais que você conectou na placa
#define ROW_1 GPIO_NUM_13
#define ROW_2 GPIO_NUM_14
#define ROW_3 GPIO_NUM_27
#define ROW_4 GPIO_NUM_26

#define COL_1 GPIO_NUM_25
#define COL_2 GPIO_NUM_33
#define COL_3 GPIO_NUM_32
#define COL_4 GPIO_NUM_9 // GPIO 34 a 39 são apenas entrada, não têm pull-up interno!
// ATENÇÃO: Se for usar pinos 34, 35, 36 ou 39 nas colunas, coloque resistores de pull-up externos de 10k!
// Caso prefira usar pull-up interno, escolha outros pinos, como 19, 21, 22, 23.

static const gpio_num_t rows[4] = {ROW_1, ROW_2, ROW_3, ROW_4};
static const gpio_num_t cols[4] = {COL_1, COL_2, COL_3, COL_4};

static const char keys[4][4] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}
};

void keypad_hw_init(void)
{
    // Configura Linhas
    for (int i = 0; i < 4; i++) {
        gpio_reset_pin(rows[i]);
        gpio_set_direction(rows[i], GPIO_MODE_OUTPUT);
        gpio_set_level(rows[i], 1); // Mantém em ALTO por padrão
    }

    // Configura Colunas
    for (int i = 0; i < 4; i++) {
        gpio_reset_pin(cols[i]);
        gpio_set_direction(cols[i], GPIO_MODE_INPUT);
        gpio_set_pull_mode(cols[i], GPIO_PULLUP_ONLY); // Requer pinos com suporte a pull-up interno
    }
}

char keypad_scan(void)
{
    char key_pressed = '\0';
    
    for (int r = 0; r < 4; r++) {
        // Coloca a linha atual em nível BAIXO
        gpio_set_level(rows[r], 0);
        
        // Pequeno delay para a capacitância parasita do cabo descarregar
        vTaskDelay(pdMS_TO_TICKS(2));

        for (int c = 0; c < 4; c++) {
            if (gpio_get_level(cols[c]) == 0) { // Detectou botão!
                key_pressed = keys[r][c];
                
                // Anti-repique (debounce) rudimentar: Trava até soltar o botão
                while (gpio_get_level(cols[c]) == 0) {
                    vTaskDelay(pdMS_TO_TICKS(20));
                }
            }
        }
        
        // Retorna a linha para nível ALTO
        gpio_set_level(rows[r], 1);
        
        if (key_pressed != '\0') {
            break; // Já achou uma tecla, pode sair
        }
    }
    
    return key_pressed;
}