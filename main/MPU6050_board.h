
// MPU6050_board.h

#ifndef MPU6050_BOARD
#define MPU6050_BOARD

#ifdef __cplusplus
extern "C"
{
#endif

// https://cdn.shopify.com/s/files/1/1509/1638/files/ESP-32_NodeMCU_Developmentboard_Pinout.pdf?v=1609851295

#define I2C_PORT -1 // -1 for auto select
#define MPU6050_I2C_SDA_PIN 21
#define MPU6050_I2C_SCL_PIN 22
#define MPU6050_I2C_SPEED 100000 // 100 kHz for standard mode.
#define MPU_INT_GPIO GPIO_NUM_27 // via driver/gpio.h

#ifdef __cplusplus
}
#endif

#endif
