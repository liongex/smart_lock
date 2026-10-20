#include "driver_servo.h"
#include "driver/mcpwm_prelude.h"
#include "esp_log.h"
#include "driver/gpio.h"

static const char *TAG = "SERVO_DRV";

// PINO DO SERVO (Ajuste para o pino da sua bancada)
#define SERVO_PIN GPIO_NUM_15

// Configurações exatas em microssegundos para o motor
#define SERVO_MIN_PULSEWIDTH_US 500  // 0 graus
#define SERVO_MAX_PULSEWIDTH_US 2500 // 180 graus
#define SERVO_TIMEBASE_RESOLUTION_HZ 1000000 // 1MHz, 1us por tick
#define SERVO_TIMEBASE_PERIOD        20000   // 20000 ticks = 20ms (50Hz)

// Variável global para armazenar o comparador (que altera a posição do motor)
static mcpwm_cmpr_handle_t comparator = NULL;

void servo_hw_init(void)
{
    ESP_LOGI(TAG, "Inicializando MCPWM para o Servo Motor...");

    // 1. Configura o Timer (Base de tempo)
    mcpwm_timer_handle_t timer = NULL;
    mcpwm_timer_config_t timer_config = {
        .group_id = 0,
        .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
        .resolution_hz = SERVO_TIMEBASE_RESOLUTION_HZ,
        .period_ticks = SERVO_TIMEBASE_PERIOD,
        .count_mode = MCPWM_TIMER_COUNT_MODE_UP,
    };
    ESP_ERROR_CHECK(mcpwm_new_timer(&timer_config, &timer));

    // 2. Configura o Operador
    mcpwm_oper_handle_t oper = NULL;
    mcpwm_operator_config_t operator_config = {
        .group_id = 0, // Mesmo grupo do timer
    };
    ESP_ERROR_CHECK(mcpwm_new_operator(&operator_config, &oper));
    ESP_ERROR_CHECK(mcpwm_operator_connect_timer(oper, timer));

    // 3. Configura o Comparador (Quem define a largura do pulso)
    mcpwm_comparator_config_t comparator_config = {
        .flags.update_cmp_on_tez = true,
    };
    ESP_ERROR_CHECK(mcpwm_new_comparator(oper, &comparator_config, &comparator));

    // 4. Configura o Gerador (Quem liga o sinal no GPIO)
    mcpwm_gen_handle_t generator = NULL;
    mcpwm_generator_config_t generator_config = {
        .gen_gpio_num = SERVO_PIN,
    };
    ESP_ERROR_CHECK(mcpwm_new_generator(oper, &generator_config, &generator));

    // 5. Define o comportamento da onda PWM
    // Vai para ALTO quando o timer zerar, e vai para BAIXO quando atingir o valor do comparador
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_timer_event(generator,
                    MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)));
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_compare_event(generator,
                    MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, comparator, MCPWM_GEN_ACTION_LOW)));

    // Inicia na posição trancada (0 graus)
    ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparator, SERVO_MIN_PULSEWIDTH_US));

    // 6. Habilita e liga o Timer
    ESP_ERROR_CHECK(mcpwm_timer_enable(timer));
    ESP_ERROR_CHECK(mcpwm_timer_start_stop(timer, MCPWM_TIMER_START_NO_STOP));
    
    ESP_LOGI(TAG, "Servo MCPWM inicializado e travado em 0 graus.");
}

void servo_lock(void)
{
    ESP_LOGI(TAG, "Acionando Servo: TRANCADO (0 graus)");
    // Passa o valor exato de 500us
    ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparator, SERVO_MIN_PULSEWIDTH_US));
}

void servo_unlock(void)
{
    ESP_LOGI(TAG, "Acionando Servo: DESTRANCADO (180 graus)");
    // Passa o valor exato de 2500us
    ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparator, SERVO_MAX_PULSEWIDTH_US));
}