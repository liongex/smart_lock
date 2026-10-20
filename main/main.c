#include <stdio.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "rtos_manager.h" // Inclui o nosso gerenciador de tarefas

static const char *TAG = "MAIN";

void app_main(void)
{
    ESP_LOGI(TAG, "Inicializando a Fechadura Inteligente...");

    // Inicializa a memória não-volátil (NVS)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "NVS Inicializada. Transferindo controle para o RTOS Manager.");

    // Chama a função de inicialização do nosso componente
    rtos_system_init();
}