idf_component_register(SRCS "driver_rfid.c"
                       INCLUDE_DIRS "include"
                       REQUIRES driver freertos rc522)