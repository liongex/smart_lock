#include "driver_display.h"
#include "ssd1306.h"
#include "esp_log.h"
#include <string.h>

// ATENÇÃO: SCL movido para GPIO 17 para não colidir com o Reset do RFID!
#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 22

static const char *TAG = "DISPLAY_DRV";
static SSD1306_t dev;

void display_hw_init(void)
{
    ESP_LOGI(TAG, "Inicializando Display OLED I2C...");
    
    // A biblioteca nopnop2002 configura o hardware do I2C para nós.
    // O -1 no último parâmetro indica que não estamos usando pino físico de Reset na tela.
    i2c_master_init(&dev, I2C_SDA_PIN, I2C_SCL_PIN, -1);
    
    // Inicializa para uma tela padrão 128x64
    ssd1306_init(&dev, 128, 64);
    
    // Limpa a memória e seta brilho no máximo
    ssd1306_clear_screen(&dev, false);
    ssd1306_contrast(&dev, 0xFF); 
    
    display_show_message("SISTEMA", "Iniciando...");
}

void display_show_message(const char* line1, const char* line2)
{
    // Apaga o que estava escrito antes
    ssd1306_clear_screen(&dev, false);
    
    // Escreve as duas linhas (a fonte suporta até 16 caracteres de largura por linha)
    if (line1 != NULL) {
        ssd1306_display_text(&dev, 2, (char*)line1, strlen(line1), false);
    }
    if (line2 != NULL) {
        ssd1306_display_text(&dev, 5, (char*)line2, strlen(line2), false);
    }
}

void display_power_off(void)
{
    // Em displays OLED, tela limpa significa pixels apagados e consumo mínimo.
    ssd1306_clear_screen(&dev, false);
}

void display_power_on(void)
{
    // Como apenas apagamos os pixels, não é necessário enviar comando de "Acordar" 
    // para o controlador. O próprio ato de escrever na tela (que o RTOS Manager 
    // faz logo em seguida) já acenderá os pixels necessários.
}