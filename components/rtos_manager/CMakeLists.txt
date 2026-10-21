idf_component_register(SRCS "rtos_manager.c"
                       INCLUDE_DIRS "include"
                       REQUIRES driver_pir driver_keypad driver_rfid auth_manager driver_servo driver_display driver freertos esp_hw_support)