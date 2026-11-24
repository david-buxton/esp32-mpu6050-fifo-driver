idf_component_register(
    SRCS "acc_driver.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "../../main"
    REQUIRES esp_common
    PRIV_REQUIRES esp_driver_i2c esp_driver_gpio
)
