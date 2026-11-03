<div align="center">

# 🔐 ESP32 Smart Lock — 2FA & Low Power

[![ESP-IDF](https://img.shields.io/badge/ESP--IDF-v5.5.1-E7352C?logo=espressif&logoColor=white)](https://docs.espressif.com/projects/esp-idf/en/v5.5.1/)
[![FreeRTOS](https://img.shields.io/badge/RTOS-FreeRTOS-informational?logo=FreeRTOS&logoColor=white)](https://www.freertos.org/)
[![Language](https://img.shields.io/badge/Language-C99-blue?logo=c&logoColor=white)]()
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![Release](https://img.shields.io/badge/Release-v1.0.0-blueviolet)]()

Um sistema seguro de fechadura inteligente autônoma desenvolvido para o microcontrolador **ESP32**, projetado em arquitetura modular limpa com **ESP-IDF v5.5.1**, **FreeRTOS** nativo, autenticação em dois fatores (**2FA: RFID + PIN**) e gestão avançada de energia (**Light Sleep** acionado por PIR).

</div>

---

## 📌 Sumário
- [Visão Geral](#-visão-geral)
- [Principais Funcionalidades](#-principais-funcionalidades)
- [Arquitetura de Hardware e Pinagem](#-arquitetura-de-hardware-e-pinagem)
- [Arquitetura de Software](#-arquitetura-de-software)
- [Máquina de Estados e IPC (FreeRTOS)](#-máquina-de-estados-e-ipc-freertos)
- [Gestão de Energia (Power Management)](#-gestão-de-energia-power-management)
- [Pré-requisitos e Dependências](#-pré-requisitos-e-dependências)
- [Compilação e Gravação](#-compilação-e-gravação)
- [Estrutura de Pastas](#-estrutura-de-pastas)
- [Roadmap (v2.0.0)](#-roadmap-v200)

---

## 📖 Visão Geral

O **ESP32 Smart Lock v1.0.0** é um sistema embarcado crítico voltado para controle de acesso físico confiável e eficiente. O projeto prioriza robustez mecânica e lógica:
- Comunicações entre subsistemas sem bloqueios (`non-blocking IPC`).
- Isolamento total entre **regras de negócio** e **drivers de hardware (HAL)**.
- Operação em modo de baixo consumo elétrico contínuo quando ocioso, acordando instantaneamente sob presença humana via interrupção externa.

---

## 🚀 Principais Funcionalidades

- **Autenticação em Dois Fatores (2FA):** 
  - Fator 1: Leitura de UID de cartão/tag RFID via **MFRC522** (Barramento SPI).
  - Fator 2: Validação de código PIN numérico via teclado matricial 4x4.
- **Acionamento Preciso da Tranca:** Controle de servomotor angular via novo driver **MCPWM** (`mcpwm_prelude.h`), garantindo posicionamento exato de travamento ($0^\circ$) e liberação ($180^\circ$).
- **Interface Visual:** Display OLED SSD1306 ($128 \times 64$) exibindo o status operacional do sistema, máscara de digitação de PIN e orientações de navegação.
- **Gestão de Energia (Light Sleep):** Modo de sono leve ativado automaticamente após 20 segundos de inatividade, preservando periféricos e acordando via interrupção de hardware por sensor de presença PIR.

---

## 🛠 Arquitetura de Hardware e Pinagem

### Componentes Utilizados
- **MCU:** ESP32 DevKitC v4 (Dual-Core Tensilica Xtensa 32-bit LX6).
- **Leitor RFID:** MFRC522 (13.56 MHz, interface SPI).
- **Display:** OLED 0.96" I2C SSD1306.
- **Teclado:** Matricial de membrana 4x4.
- **Sensor de Presença:** PIR HC-SR501 ou similar (saída digital com nível lógico alto em detecção).
- **Atuador:** Servomotor SG90 / MG996R acoplado ao mecanismo de tranca.

### Mapa de Pinagem (Pinout)

| Componente | Sinal / Função | GPIO ESP32 | Notas / Configuração |
| :--- | :--- | :---: | :--- |
| **Sensor PIR** | Wake-Up Signal | **GPIO 4** | Configurado como fonte de wake-up em nível HIGH |
| **Display OLED** | I2C SDA | **GPIO 21** | Barramento I2C compartilhado / `CONFIG_LEGACY_DRIVER=y` |
| | I2C SCL | **GPIO 22** | Frequência: 400 kHz |
| **Servo Motor** | Sinal PWM | **GPIO 15** | MCPWM (500 $\mu$s = $0^\circ$, 2500 $\mu$s = $180^\circ$) |
| **Leitor RFID** | SPI MOSI | **GPIO 23** | Barramento VSPI/SPI2 |
| | SPI MISO | **GPIO 19** | |
| | SPI SCK | **GPIO 18** | |
| | SPI CS (SDA) | **GPIO 5** | Chip Select |
| | Reset (RST) | **GPIO 17** | Pino de reset de hardware |
| **Teclado 4x4** | Linhas (L1–L4) | **GPIOs 32, 33, 25, 26** | Saídas digitais em varredura sequencial |
| | Colunas (C1–C4) | **GPIOs 27, 14, 12, 13** | Entradas com pull-down interno habilitado |

---

## 🏛 Arquitetura de Software

O código-fonte segue as diretrizes de **Arquitetura Limpa (Clean Architecture)** e modularização por componentes nativos do ESP-IDF:

```
┌─────────────────────────────────────────────────────────────┐
│                            main.c                           │
│             (Inicialização da NVS e chamada do RTOS)        │
└──────────────────────────────┬──────────────────────────────┘
                               │
                ┌──────────────▼─────────────┐
                │        rtos_manager        │
                │ (Orquestração, IPC & Sleep)│
                └──────────────┬─────────────┘
                               │
                ┌──────────────▼─────────────┐
                │        auth_manager        │
                │  (FSM de 2FA & Validação)  │
                └──────────────┬─────────────┘
                               │
     ┌─────────────────────────┼─────────────────────────┐
     │                         │                         │
┌────▼────────┐          ┌─────▼───────┐          ┌──────▼──────┐
│ driver_rfid │          │driver_keypad│          │ driver_servo│
└─────────────┘          └─────────────┘          └─────────────┘
┌─────────────┐          ┌─────────────┐
│driver_pir   │          │driver_display│
└─────────────┘          └─────────────┘
```

- **`main`**: Ponto de entrada; inicializa subsistemas básicos e transfere o controle.
- **`rtos_manager`**: Camada de orquestração. Inicializa os drivers, cria tarefas e gerencia filas, grupos de eventos e timers do FreeRTOS.
- **`auth_manager`**: Módulo de regras de negócio. Implementa a máquina de estados finita (FSM) de autenticação e valida credenciais.
- **`driver_*` (HAL)**: Drivers de hardware desacoplados. Não possuem decisões de negócio; expõem interfaces limpas para leitura de sensores e comando de atuadores.

---

## ⚙ Máquina de Estados e IPC (FreeRTOS)

O fluxo operacional é assíncrono e guiado por eventos, sem laços de atraso bloqueante (`vTaskDelay`) na tomada de decisão:

```
             ┌─────────────────────────┐
             │        ESTADO 0         │
             │   Dormir (Light Sleep)  │
             └────────────┬────────────┘
                          │ PIR Wake-up (Interrupção)
                          ▼
             ┌─────────────────────────┐
             │        ESTADO 1         │ ◄───┐
      ┌────► │   Aguardando Cartão     │     │ Falha PIN / Timeout
      │      └────────────┬────────────┘     │
Timeout (20s)             │ RFID Válido      │
      │                   ▼                  │
      │      ┌─────────────────────────┐     │
      └───── │        ESTADO 2         ├─────┘
             │      Autenticação       │
             │      (Senha / PIN)      │
             └────────────┬────────────┘
                          │ PIN Correto
                          ▼
             ┌─────────────────────────┐
             │        ESTADO 3         │
             │     Acesso Liberado     │
             │  (Tranca 180° por 3 s)  │
             └────────────┬────────────┘
                          │ Conclusão do ciclo
                          ▼
                    (Retorna ao Estado 0)
```

### Mecanismos de Comunicação Inter-Processos (IPC)
- **`q_rfid` (Queue):** Envia a UID lida pelo leitor RFID para a tarefa de autenticação de forma assíncrona.
- **`q_keypad` (Queue):** Fila de caracteres (teclas pressionadas) enviadas à máquina de estados.
- **`system_events` (EventGroup):** Gerencia flags de sincronização (ex.: `BIT_SYSTEM_AWAKE`).
- **`sleep_timer` (Software Timer):** Temporizador de 20 segundos resetado a cada interação do usuário; se estourar, o sistema entra em sono profundo/leve com segurança.

---

## 🔋 Gestão de Energia (Power Management)

No **Estado 0**, o microcontrolador suspende as tarefas ativas e entra em **ESP Light Sleep**:
- O display OLED é desligado para corte de corrente no painel.
- O subsistema de RF do MFRC522 é pausado.
- Os clocks internos da CPU são reduzidos ou desligados, retendo o estado de memória RAM e registradores.
- O GPIO do sensor PIR atua como trigger de interrupção externa (`esp_sleep_enable_gpio_wakeup`), restaurando o estado de execução em microsegundos ao identificar aproximação de pessoas.

---

## 📦 Pré-requisitos e Dependências

- **ESP-IDF v5.5.1** (instalado e configurado no ambiente PATH).
- **Python 3.10+**.
- **Bibliotecas gerenciadas / componentes:**
  - `abobija/rc522`: Driver do módulo RFID MFRC522 via SPI.
  - `nopnop2002/esp-idf-ssd1306`: Driver para o display OLED I2C.

> **Importante:** Devido a particularidades de coexistência entre bibliotecas de periféricos I2C no ESP-IDF v5.x, utilize `CONFIG_LEGACY_DRIVER=y` no `sdkconfig` para habilitar a camada de compatibilidade estável do driver de I2C.

---

## 🔨 Compilação e Gravação

1. **Clone o repositório:**
   ```bash
   git clone https://github.com/seu-usuario/esp32-smart-lock.git
   cd esp32-smart-lock
   ```

2. **Configure o ambiente do ESP-IDF (Linux/macOS):**
   ```bash
   . $HOME/esp/esp-idf/export.sh
   ```

3. **Selecione o target do microcontrolador:**
   ```bash
   idf.py set-target esp32
   ```

4. **Compile o projeto:**
   ```bash
   idf.py build
   ```

5. **Grave a aplicação e abra o monitor serial:**
   ```bash
   idf.py -p /dev/ttyUSB0 flash monitor
   ```
   *(Substitua `/dev/ttyUSB0` pela porta serial da sua placa; no Windows, use `COMx`).*

---

## 📂 Estrutura de Pastas

```text
smart_lock/
├── CMakeLists.txt
├── sdkconfig
├── main/
│   ├── CMakeLists.txt
│   └── main.c
└── components/
    ├── auth_manager/          # Máquina de estados 2FA e banco de credenciais
    ├── rtos_manager/          # Orquestração do FreeRTOS, IPC e Power Management
    ├── driver_display/        # Abstração do display SSD1306
    ├── driver_keypad/         # Varredura e debouncing do teclado 4x4
    ├── driver_pir/            # Driver do sensor PIR e configuração de wake-up
    ├── driver_rfid/           # Driver do leitor MFRC522 SPI
    ├── driver_servo/          # Driver MCPWM para atuador da tranca
    └── ssd1306/               # Biblioteca base do display
```

---

## 🗺 Roadmap (v2.0.0)

- [ ] **NVS Dinâmica:** Cadastro, remoção e persistência de cartões e senhas na memória flash (eliminando credenciais *hardcoded*).
- [ ] **Conectividade Wi-Fi & MQTT:** Envio de logs de acesso e alertas de violação em tempo real.
- [ ] **Provisionamento BLE / Web:** Interface de configuração local sem necessidade de recompilar firmware.
- [ ] **Criptografia de PIN:** Hashing das senhas antes do armazenamento e validação.

---

## 📄 Licença

Distribuído sob a licença MIT. Consulte o arquivo `LICENSE` para mais informações.
