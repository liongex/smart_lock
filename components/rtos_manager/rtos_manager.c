#include <stdio.h>
#include "esp_log.h"
#include "esp_sleep.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/timers.h"
#include "driver/gpio.h"

#include "rtos_manager.h"
#include "driver_pir.h"

#define PIR_GPIO GPIO_NUM_4 // Altere para o GPIO onde ligou o PIR

static const char *TAG = "RTOS_MGR";

EventGroupHandle_t system_events;
TimerHandle_t sleep_timer;
TaskHandle_t power_task_handle = NULL;

// Callback chamado quando passa 20 segundos sem movimento
void sleep_timer_callback(TimerHandle_t xTimer)
{
    ESP_LOGW(TAG, "Timeout de inatividade! Preparando para dormir...");
    
    // Limpa o bit (as tasks de varredura vao parar)
    xEventGroupClearBits(system_events, BIT_SYSTEM_AWAKE);
    
    // Avisa a tarefa de power management para chamar a função de sleep
    xTaskNotifyGive(power_task_handle);
}

// Tarefa dedicada a gerenciar a energia
void task_power_management(void *pvParameters)
{
    while (1) {
        ESP_LOGI(TAG, "Configurando Wake-up e entrando em Light Sleep...");
        
        // Dá um pequeno atraso para os logs terminarem de ser impressos no console
        vTaskDelay(pdMS_TO_TICKS(100)); 
        
        // Habilita o pino do PIR para acordar o ESP32 em nível ALTO
        gpio_wakeup_enable(PIR_GPIO, GPIO_INTR_HIGH_LEVEL);
        esp_sleep_enable_gpio_wakeup();
        
        // ----- A CPU TRAVA NESTA LINHA ATÉ O PIR DETECTAR MOVIMENTO -----
        esp_light_sleep_start();
        
        // ----- O ESP32 ACORDA AQUI -----
        gpio_wakeup_disable(PIR_GPIO); // Desativa para não ficar gerando interrupção à toa
        ESP_LOGI(TAG, "Sistema Acordou! Movimento detectado.");
        
        // Sinaliza para o resto do sistema (Teclado, RFID) que podem rodar
        xEventGroupSetBits(system_events, BIT_SYSTEM_AWAKE);
        
        // Inicia (ou reseta) o timer de 20 segundos
        xTimerStart(sleep_timer, 0);
        
        // Bloqueia esta tarefa até que o Timer estoure e envie a notificação
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    }
}

void rtos_system_init(void)
{
    ESP_LOGI(TAG, "Inicializando RTOS e Hardware Base...");

    // Inicializa primitivas do FreeRTOS
    system_events = xEventGroupCreate();
    
    // Cria o Timer de 20 segundos (20000 ms), pdFALSE significa "Não repete automaticamente"
    sleep_timer = xTimerCreate("SleepTimer", pdMS_TO_TICKS(20000), pdFALSE, NULL, sleep_timer_callback);

    // Inicializa hardware
    pir_hw_init(PIR_GPIO);

    // Cria a tarefa de Power Management com alta prioridade
    xTaskCreate(task_power_management, "Power_Mgr", 3072, NULL, 6, &power_task_handle);
}