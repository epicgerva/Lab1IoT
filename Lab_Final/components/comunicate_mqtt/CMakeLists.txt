idf_component_register(SRCS "comunicate_mqtt.c"
                    INCLUDE_DIRS "include"
                    REQUIRES mqtt nvs_flash json player logger ntp)
