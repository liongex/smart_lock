#include <stdio.h>
#include "esp_log.h"
#include "esp_sleep.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "freertos/timers.h"
#include "driver/gpio.h"

#include "rtos_manager.h"
#include "driver_pir.h"
#include "driver_keypad.h"
#include "driver_rfid.h"

#include "auth_manager.h"

// TODO: Confirme se o pino do PIR está no GPIO 4 na sua bancada
#define PIR_GPIO GPIO_NUM_4 

static const char *TAG = "RTOS_MGR";

// Handles do FreeRTOS
EventGroupHandle_t system_events;
TimerHandle_t sleep_timer;
TaskHandle_t power_task_handle = NULL;

// Filas para os eventos de interface
QueueHandle_t q_keypad_char;
QueueHandle_t q_rfid_uid;

// -----------------------------------------------------------------------------
// CALLBACKS
// -----------------------------------------------------------------------------

// Callback chamado pelo Timer do FreeRTOS após 20 segundos de inatividade
void sleep_timer_callback(TimerHandle_t xTimer)
{
    ESP_LOGW(TAG, "Timeout de inatividade! Preparando para dormir...");
    
    // Limpa o bit (as tasks de varredura param de rodar)
    xEventGroupClearBits(system_events, BIT_SYSTEM_AWAKE);
    
    // Avisa a tarefa de power management para comandar o Light Sleep
    xTaskNotifyGive(power_task_handle);
}

// Callback chamado automaticamente pelo driver_rfid quando um cartão é lido na SPI
void on_card_scanned(uint64_t uid)
{
    // Verifica se o sistema está acordado (evita varreduras falsas ou ruído enquanto dorme)
    EventBits_t uxBits = xEventGroupGetBits(system_events);
    if ((uxBits & BIT_SYSTEM_AWAKE) != 0) {
        
        // Envia para a fila com tempo de espera 0. Se a fila estiver cheia, apenas descarta.
        if (xQueueSend(q_rfid_uid, &uid, 0) == pdPASS) {
            ESP_LOGI(TAG, "UID %llu enviado para a fila de Autenticacao.", uid);
            
            // O usuário agiu, então resetamos o timer de inatividade (mais 20 seg)
            xTimerReset(sleep_timer, 0);
        } else {
            ESP_LOGW(TAG, "Fila RFID cheia! UID ignorado.");
        }
    }
}

// -----------------------------------------------------------------------------
// TAREFAS (TASKS)
// -----------------------------------------------------------------------------

// Tarefa dedicada a orquestrar a entrada e saída do modo de economia de energia
void task_power_management(void *pvParameters)
{
    while (1) {
        ESP_LOGI(TAG, "Configurando Wake-up e entrando em Light Sleep...");
        
        // Dá um pequeno atraso para os logs terminarem de ser enviados ao PC
        vTaskDelay(pdMS_TO_TICKS(100)); 
        
        // 1. Prepara periféricos para dormir
        rfid_hw_pause(); // Congela a tarefa do rc522 e corta energia no pino RST
        
        // 2. Configura a interrupção do PIR para acordar o ESP32 em nível ALTO
        gpio_wakeup_enable(PIR_GPIO, GPIO_INTR_HIGH_LEVEL);
        esp_sleep_enable_gpio_wakeup();
        
        // ----- A CPU PARA NESTA LINHA ATÉ O PIR DETECTAR MOVIMENTO -----
        esp_light_sleep_start();
        
        // ----- O ESP32 ACORDA AQUI -----
        gpio_wakeup_disable(PIR_GPIO); // Desativa para não gerar interrupção à toa
        
        // 3. Acorda periféricos
        rfid_hw_resume(); // Restaura energia do rc522 e retoma tarefa interna dele
        
        ESP_LOGI(TAG, "Sistema Acordou! Movimento detectado.");
        
        // 4. Sinaliza para a task do teclado que ela pode voltar a varrer os pinos
        xEventGroupSetBits(system_events, BIT_SYSTEM_AWAKE);
        
        // 5. Inicia (ou reseta) o timer de inatividade
        xTimerStart(sleep_timer, 0);
        
        // 6. Bloqueia esta tarefa de energia até o Timer estourar e enviar a notificação
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    }
}

// Tarefa de varredura das matrizes de linhas e colunas do teclado
void task_keypad_scan(void *pvParameters)
{
    char key;
    while(1) {
        // Regra de Ouro: Fica bloqueada aqui sem consumir CPU se o sistema estiver dormindo
        xEventGroupWaitBits(system_events, BIT_SYSTEM_AWAKE, pdFALSE, pdTRUE, portMAX_DELAY);
        
        key = keypad_scan();
        
        if (key != '\0') {
            ESP_LOGI("KEYPAD", "Tecla lida: %c", key);
            
            // Envia para a fila (timeout 0 para evitar deadlock de fila cheia)
            if (xQueueSend(q_keypad_char, &key, 0) == pdPASS) {
                // Ação detectada: ganha mais 20 segundos antes de dormir
                xTimerReset(sleep_timer, 0);
            } else {
                ESP_LOGW("KEYPAD", "Fila cheia! Tecla %c descartada.", key);
            }
        }
        
        // Polling da matriz a cada 50ms para efeito de debounce de hardware
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// -----------------------------------------------------------------------------
// INICIALIZAÇÃO
// -----------------------------------------------------------------------------

void rtos_system_init(void)
{
    ESP_LOGI(TAG, "Inicializando RTOS e Hardware Base...");

    // 1. Cria os objetos de sincronização IPC do FreeRTOS
    system_events = xEventGroupCreate();
    q_keypad_char = xQueueCreate(10, sizeof(char));
    q_rfid_uid    = xQueueCreate(5, sizeof(uint64_t)); // Fila para UIDs do RC522
    
    sleep_timer = xTimerCreate("SleepTimer", pdMS_TO_TICKS(20000), pdFALSE, NULL, sleep_timer_callback);

    // 2. Inicializa o nível de hardware (Pinos e Periféricos)
    pir_hw_init(PIR_GPIO);
    keypad_hw_init();
    
    // Injeta a função callback dentro do driver SPI do RC522
    rfid_hw_init(on_card_scanned); 

    // 3. Cria as Tarefas de Sistema
    // Obs: A tarefa da varredura do RFID já é criada internamente pela biblioteca 'abobija'
    xTaskCreate(task_power_management, "Power_Mgr",   3072, NULL, 6, &power_task_handle);
    xTaskCreate(task_keypad_scan,      "Keypad_Scan", 2048, NULL, 5, NULL);

    // NOVO: Inicializa a máquina de estados da autenticação!
    auth_manager_init(q_rfid_uid, q_keypad_char);
}