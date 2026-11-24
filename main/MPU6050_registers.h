
// MPU6050_registers.h

#ifndef MPU6050_REGISTERS
#define MPU6050_REGISTERS

#ifdef __cplusplus
extern "C"
{
#endif

// Register addresses

#define WHO_AM_I 0x75
#define MPU6050_ID 0x68 // expected contents given an official chip
#define PWR_MGMT_1 0x6B
#define ACCEL_XOUT_H 0x3B
#define CONFIG 0x1A     // for accelerometer sampling rate
#define SMPLRT_DIV 0x19 // for accelerometer sampling rate
#define ACCEL_CONFIG 0x1C

/* Register addresses for FIFO buffer */

#define USER_CTRL 0x6A
#define FIFO_EN 0x23
#define FIFO_COUNTH 0x72
#define FIFO_R_W 0x74
#define INT_STATUS 0x3A

/* For the interrupt */

#define INT_PIN_CFG 0x37
#define INT_ENABLE 0x38

// Bit masks

#define MPU6050_SLEEP_BIT (1U << 6)

// Bit masks and register for enabling FIFO

#define MPU6050_FIFO_DISABLE 0x00
#define MPU6050_FIFO_RESET (1U << 2)
#define MPU6050_FIFO_ENABLE (1U << 6)       // Enable FIFO hardware
#define MPU6050_ACCEL_FIFO_ENABLE (1U << 3) // Enable accelerometer FIFO writes

#ifdef __cplusplus
}
#endif

#endif
