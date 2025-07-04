idf_component_register(SRCS "player.c"
                      INCLUDE_DIRS "include" "../../"
                      PRIV_REQUIRES esp_driver_i2s esp_driver_gpio
                      REQUIRES playlist es8311 logger led delay)
