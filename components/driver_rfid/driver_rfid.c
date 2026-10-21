#include "driver_rfid.h"
#include "rc522.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "RFID_DRV";

#define PIN_NUM_MISO 19
#define PIN_NUM_MOSI 23
#define PIN_NUM_CLK  18
#define PIN_NUM_CS   5
#define PIN_NUM_RST  17 

static rc522_handle_t scanner;
static rfid_scanned_cb_t app_callback = NULL;

// Função interna que é chamada pela biblioteca abobija quando algo acontece
static void rc522_handler(void* arg, esp_event_base_t base, int32_t event_id, void* event_data)
{
    rc522_event_data_t* data = (rc522_event_data_t*) event_data;

    if (event_id == RC522_EVENT_TAG_SCANNED) {
        rc522_tag_t* tag = (rc522_tag_t*) data->ptr;
        
        ESP_LOGI(TAG, "Cartão lido no hardware! UID: %llu", tag->serial_number);

        // Se o RTOS Manager nos passou uma função, nós a chamamos agora enviando o UID
        if (app_callback != NULL) {
            app_callback(tag->serial_number);
        }
    }
}

void rfid_hw_init(rfid_scanned_cb_t callback)
{
    ESP_LOGI(TAG, "Inicializando RC522 com a biblioteca abobija...");
    app_callback = callback;

    // Garante que o pino de Reset físico não fique flutuando
    gpio_reset_pin(PIN_NUM_RST);
    gpio_set_direction(PIN_NUM_RST, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_NUM_RST, 1);

    rc522_config_t config = {
        .spi.host = SPI2_HOST,
        .spi.miso_gpio = PIN_NUM_MISO,
        .spi.mosi_gpio = PIN_NUM_MOSI,
        .spi.sck_gpio = PIN_NUM_CLK,
        .spi.sda_gpio = PIN_NUM_CS,
    };

    rc522_create(&config, &scanner);
    rc522_register_events(scanner, RC522_EVENT_ANY, rc522_handler, NULL);
    
    // A biblioteca abobija cria a própria task no FreeRTOS aqui
    rc522_start(scanner); 
}

void rfid_hw_pause(void)
{
    ESP_LOGD(TAG, "Pausando tarefa interna do RC522...");
    rc522_pause(scanner);
    
    // Coloca o chip em Power Down físico para máxima economia
    //gpio_set_level(PIN_NUM_RST, 0); 
}

void rfid_hw_resume(void)
{
    ESP_LOGD(TAG, "Acordando RC522...");
    
    // Restaura a energia do chip
    //gpio_set_level(PIN_NUM_RST, 1);
    
    // Reinicia a varredura da biblioteca
    rc522_start(scanner); 
}