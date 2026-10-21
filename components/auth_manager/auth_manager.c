#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "auth_manager.h"
#include "driver_servo.h"
#include "driver_display.h"

static const char *TAG = "AUTH_LOGIC";

// Estrutura do Cofre de Credenciais
typedef struct {
    uint64_t uid;
    char pin[5]; // 4 dígitos da senha + caractere nulo '\0'
} UserCredential_t;

// Banco de dados local (Hardcoded por enquanto)
static const UserCredential_t valid_users[] = {
    // COLOQUE O UID DO SEU CARTÃO AQUI E ESCOLHA UMA SENHA
    { 441324254953, "1234" }, 
    { 590270235196, "ABCD" }
};
#define NUM_USERS (sizeof(valid_users) / sizeof(valid_users[0]))

// Variáveis locais para armazenar as filas recebidas na inicialização
static QueueHandle_t q_rfid;
static QueueHandle_t q_keypad;

void task_auth_logic(void *pvParameters)
{
    uint64_t current_uid = 0;
    char input_pin[5] = {0};
    int pin_index = 0;
    int user_index = -1;

    while (1) {
        if (xQueueReceive(q_rfid, &current_uid, portMAX_DELAY) == pdTRUE) {
            
            user_index = -1;
            for (int i = 0; i < NUM_USERS; i++) {
                if (valid_users[i].uid == current_uid) {
                    user_index = i;
                    break;
                }
            }

            if (user_index == -1) {
                display_show_message("ACESSO NEGADO", "Cartao Invalido!");
                vTaskDelay(pdMS_TO_TICKS(2000));
                display_show_message("SISTEMA ATIVO", "Aproxime o Cartao");
                continue; 
            }

            display_show_message("CARTAO LIDO", "Digite o PIN:");
            pin_index = 0;
            memset(input_pin, 0, sizeof(input_pin)); 
            xQueueReset(q_keypad); 

            bool timeout = false;
            
            while (pin_index < 4) {
                char key;
                if (xQueueReceive(q_keypad, &key, pdMS_TO_TICKS(10000)) == pdTRUE) {
                    
                    if (key == '*') {
                        pin_index = 0;
                        display_show_message("CARTAO LIDO", "Digite o PIN:");
                    } else { 
                        input_pin[pin_index] = key;
                        pin_index++;
                        
                        // Cria a animação de asteriscos (ex: "***")
                        char pin_stars[5] = {0};
                        for (int i = 0; i < pin_index; i++) pin_stars[i] = '*';
                        display_show_message("Digite o PIN:", pin_stars);
                    }
                } else {
                    timeout = true;
                    break;
                }
            }

            if (timeout) {
                display_show_message("TEMPO ESGOTADO", "Cancelado.");
                vTaskDelay(pdMS_TO_TICKS(2000));
                display_show_message("SISTEMA ATIVO", "Aproxime o Cartao");
                continue; 
            }

            if (strcmp(input_pin, valid_users[user_index].pin) == 0) {
                display_show_message("ACESSO LIBERADO", "Porta Aberta!");
                servo_unlock();
                vTaskDelay(pdMS_TO_TICKS(3000)); 
                servo_lock();
                display_show_message("SISTEMA ATIVO", "Aproxime o Cartao");
            } else {
                display_show_message("ACESSO NEGADO", "PIN Incorreto!");
                vTaskDelay(pdMS_TO_TICKS(2000));
                display_show_message("SISTEMA ATIVO", "Aproxime o Cartao");
            }
            
            xQueueReset(q_keypad);
        }
    }
}

void auth_manager_init(QueueHandle_t rfid_queue, QueueHandle_t keypad_queue)
{
    ESP_LOGI(TAG, "Inicializando Motor de Autenticacao...");
    
    // Salva a referência das filas
    q_rfid = rfid_queue;
    q_keypad = keypad_queue;
    
    // Cria a tarefa de autenticação. Prioridade 4 (um pouco menor que as varreduras de hardware)
    xTaskCreate(task_auth_logic, "Auth_Logic", 4096, NULL, 4, NULL);
}