idf_component_register(SRCS "main.c"
                      INCLUDE_DIRS "."
                      REQUIRES es8311 playlist player logger touch led delay wifi http comunicate_mqtt)