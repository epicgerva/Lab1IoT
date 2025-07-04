idf_component_register(SRCS "comunicate_mqtt.c"
                    INCLUDE_DIRS "include"
                    REQUIRES mbedtls 
                    REQUIRES mqtt)
