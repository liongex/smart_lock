#include <stdio.h>
#include "esp_log.h"
#include "esp_sleep.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"          // NOVO: Para criarmos a fila
#include "freertos/event_groups.h"
#include "freertos/timers.h"
#include "driver/gpio.h"

#include "rtos_manager.h"
#include "driver_pir.h"
#include "driver_keypad.h"           // NOVO: Inclusão do driver

#define PIR_GPIO GPIO_NUM_4 

static const char *TAG = "RTOS_MGR";

EventGroupHandle_t system_events;
TimerHandle_t sleep_timer;
TaskHandle_t power_task_handle = NULL;

// NOVO: Handle da fila do teclado
QueueHandle_t q_keypad_char;

// Callback chamado quando passa 20 segundos sem movimento (Mantém igual)
void sleep_timer_callback(TimerHandle_t xTimer)
{
    ESP_LOGW(TAG, "Timeout de inatividade! Preparando para dormir...");
    xEventGroupClearBits(system_events, BIT_SYSTEM_AWAKE);
    xTaskNotifyGive(power_task_handle);
}

// Tarefa de Power Management (Mantém igual, sem alterações)
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

// NOVO: Tarefa de varredura do teclado
void task_keypad_scan(void *pvParameters)
{
    char key;
    while(1) {
        // Regra de ouro: Só avança se o sistema estiver acordado. 
        // Se estiver dormindo, bloqueia indefinidamente sem gastar CPU.
        xEventGroupWaitBits(system_events, BIT_SYSTEM_AWAKE, pdFALSE, pdTRUE, portMAX_DELAY);
        
        key = keypad_scan();
        
        if (key != '\0') {
            ESP_LOGI("KEYPAD", "Tecla lida: %c", key);
            
        // Tenta enviar com tempo de espera 0. Se não conseguir (pdPASS), avisa no log.
        if (xQueueSend(q_keypad_char, &key, 0) != pdPASS) {
            ESP_LOGW("KEYPAD", "Fila cheia! Tecla %c descartada.", key);
        }
            
            // Muito importante: Reseta o timer de inatividade. 
            // Cada vez que o usuário aperta um botão, ganhamos mais 20 segundos!
            xTimerReset(sleep_timer, 0);
        }
        
        // Intervalo de polling. 50ms é o ideal para teclados mecânicos
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void rtos_system_init(void)
{
    ESP_LOGI(TAG, "Inicializando RTOS e Hardware Base...");

    system_events = xEventGroupCreate();
    
    // NOVO: Fila para armazenar até 10 teclas digitadas
    q_keypad_char = xQueueCreate(10, sizeof(char));
    
    sleep_timer = xTimerCreate("SleepTimer", pdMS_TO_TICKS(20000), pdFALSE, NULL, sleep_timer_callback);

    pir_hw_init(PIR_GPIO);
    keypad_hw_init(); // NOVO: Inicializa os pinos do teclado

    xTaskCreate(task_power_management, "Power_Mgr",   3072, NULL, 6, &power_task_handle);
    
    // NOVO: Cria a tarefa do teclado com prioridade 5 (menor que a do Power Mgr)
    xTaskCreate(task_keypad_scan,      "Keypad_Scan", 2048, NULL, 5, NULL);
}