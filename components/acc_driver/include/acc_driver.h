
//acc_driver.h

//this is the MPU6050 app layer and will call the I2C layer directly

#ifndef ACC_DRIVER
#define ACC_DRIVER

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"
#include <stdint.h>
#include <stddef.h>

/*

FIFO is read in batches of 32no 6 byte samples = 192 bytes

device_read_multiple_fifo_samples() allocates 192 bytes
drain_fifo () allocates 192 bytes
And they are called together so
Combined stack usage for these buffers = 384 bytes, approx 10% of stack allocated to acquisition_task().

Reasonable initial value balancing the need to drain FIFO quickly (transaction duration) with the risk of running out of stack

*/
#define MPU6050_MAX_BATCH_SAMPLES 32U
#define MPU6050_BYTES_PER_SAMPLE  6U // one complete mpu6050 3 - axis read

typedef struct {
  int16_t axis_reading[3];
} accel_3_axis_sample;

typedef struct {
  float axis_reading[3];
} g_acc_3_axis_sample;

/* I2C and device configuration */

esp_err_t bus_init(void);

esp_err_t device_init(void);

esp_err_t device_read_id(void);

esp_err_t device_wake(void);  //chip needs to be awake to read sensors

esp_err_t device_read_device_sensitivity_and_range(unsigned *range, unsigned *sensitivity);

esp_err_t device_configure_sampling(void);

esp_err_t access_register_read(  //public helper for testing
    uint8_t register_address,
    uint8_t *buffer,
    size_t length);

/* FIFO buffer configuration */

esp_err_t device_enable_fifo(void);

esp_err_t device_count_fifo_bytes(uint16_t *byte_count);

esp_err_t device_read_multiple_fifo_samples(size_t count, accel_3_axis_sample *sample);

esp_err_t check_and_reset_FIFO_overflow(void);

/* Interrupt configuration */

esp_err_t configure_interrupt_gpio(void);

esp_err_t device_start_acquisition_task(void);

esp_err_t device_enable_interrupt(void);

/* Public helpers for testing access */

esp_err_t device_suspend_acquisition(void);

esp_err_t device_resume_acquisition(void);

#ifdef __cplusplus
}
#endif

#endif
