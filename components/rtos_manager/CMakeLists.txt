idf_component_register(SRCS "rtos_manager.c"
                       INCLUDE_DIRS "include"
                       REQUIRES driver_pir driver_keypad driver_rfid driver freertos esp_hw_support)