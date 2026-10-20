#include <stdio.h>
#include "esp_log.h"
#include "rtos_manager.h"

static const char *TAG = "RTOS_MGR";

void rtos_system_init(void)
{
    ESP_LOGI(TAG, "RTOS Manager acionado. (As tarefas serao criadas na Etapa 3)");
    
    // O loop infinito a seguir mantém a tarefa main() viva, 
    // mas em projetos FreeRTOS o app_main pode retornar sem problemas.
    // Deixaremos vazio por enquanto.
}