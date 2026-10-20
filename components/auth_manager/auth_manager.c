#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "auth_manager.h"
#include "driver_servo.h"

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
        // ESTADO 1: OCIOSO (Aguardando um cartão RFID)
        // Fica bloqueado aqui infinitamente sem gastar CPU até chegar um UID na fila
        if (xQueueReceive(q_rfid, &current_uid, portMAX_DELAY) == pdTRUE) {
            
            ESP_LOGI(TAG, "====================================");
            ESP_LOGI(TAG, "Tentativa de acesso. UID lido: %llu", current_uid);

            // Verifica se o cartão existe no nosso banco de dados
            user_index = -1;
            for (int i = 0; i < NUM_USERS; i++) {
                if (valid_users[i].uid == current_uid) {
                    user_index = i;
                    break;
                }
            }

            if (user_index == -1) {
                ESP_LOGW(TAG, "Cartao NAO CADASTRADO. Acesso Negado.");
                continue; // Volta para o Estado 1 (aguardar outro cartão)
            }

            // ESTADO 2: AGUARDANDO SENHA
            ESP_LOGI(TAG, "Cartao Reconhecido! Digite o PIN de 4 digitos:");
            pin_index = 0;
            memset(input_pin, 0, sizeof(input_pin)); // Limpa o buffer da senha
            
            // Limpa qualquer tecla que a pessoa tenha digitado antes de passar o cartão
            xQueueReset(q_keypad); 

            bool timeout = false;
            
            // Fica num loop aguardando 4 teclas
            while (pin_index < 4) {
                char key;
                // Aguarda até 10 segundos para a pessoa apertar uma tecla.
                if (xQueueReceive(q_keypad, &key, pdMS_TO_TICKS(10000)) == pdTRUE) {
                    
                    if (key == '*') {
                        // Mantemos o asterisco exclusivo como botão de "Corrigir/Limpar"
                        pin_index = 0;
                        ESP_LOGI(TAG, "Digitacao cancelada. Digite o PIN novamente:");
                        
                    } else { 
                        // Aceita números (0-9), letras (A-D) e até a cerquilha (#)
                        input_pin[pin_index] = key;
                        pin_index++;
                        ESP_LOGI(TAG, "Tecla registrada (*). Faltam %d", 4 - pin_index);
                    }
                    
                } else {
                    ESP_LOGW(TAG, "Timeout! Demorou muito para digitar. Cancelando...");
                    timeout = true;
                    break;
                }
            }

            if (timeout) {
                continue; // Volta para o Estado 1
            }

            // ESTADO 3: VERIFICAÇÃO DO DUPLO FATOR
            if (strcmp(input_pin, valid_users[user_index].pin) == 0) {
                ESP_LOGI(TAG, "*************************************");
                ESP_LOGI(TAG, "     ACESSO LIBERADO! Porta Aberta   ");
                ESP_LOGI(TAG, "*************************************");
                
                // NOVO: Gira o servo para 180º abrindo a tranca
                servo_unlock();
                
                // Mantém aberto por 3 segundos (tempo para a pessoa empurrar a porta)
                vTaskDelay(pdMS_TO_TICKS(3000)); 
                
                // NOVO: Volta o servo para 0º trancando a porta
                servo_lock();
                
                ESP_LOGI(TAG, "Porta trancada novamente.");
            } else {
                ESP_LOGW(TAG, "PIN INCORRETO. Acesso Negado.");
            }
            
            // Limpa a fila caso o usuário tenha apertado teclas extras após a 4ª tecla
            xQueueReset(q_keypad);
            ESP_LOGI(TAG, "====================================");
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