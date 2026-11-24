
//acc_driver.c

#include "MPU6050_registers.h"
#include "MPU6050_board.h"
#include "acc_driver.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

#include "driver/i2c_master.h"
#include "esp_attr.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static bool interrupt_gpio_configured = false;

//I2C master bus specific configuration
static const i2c_master_bus_config_t master_bus_config = {
    .clk_source = I2C_CLK_SRC_DEFAULT,
    .i2c_port = I2C_PORT,
    .scl_io_num = MPU6050_I2C_SCL_PIN,
    .sda_io_num = MPU6050_I2C_SDA_PIN,
    .glitch_ignore_cnt = 7, //default value
    .flags.enable_internal_pullup = true,
    };

//I2C device configuration
static const i2c_device_config_t device_config = {
    .dev_addr_length = I2C_ADDR_BIT_LEN_7,  //standard 7-bit addressing
    .device_address = MPU6050_ID,
    .scl_speed_hz = MPU6050_I2C_SPEED,
    };

static i2c_master_bus_handle_t master_bus_handle = NULL;
static i2c_master_dev_handle_t MPU6050_device_handle = NULL;

/* Setup for multithreading */

static TaskHandle_t acquisition_task_handle = NULL;

static esp_err_t drain_fifo(void)
    {
        accel_3_axis_sample samples[MPU6050_MAX_BATCH_SAMPLES];

        while (1) {
            uint16_t fifo_byte_count;

            esp_err_t ret = device_count_fifo_bytes(&fifo_byte_count);
            if (ret != ESP_OK) return ret;

            if (fifo_byte_count % MPU6050_BYTES_PER_SAMPLE != 0) {
            ESP_LOGW("MPU6050", "FIFO count not divisible by 6");
            }

            size_t sample_count = fifo_byte_count / MPU6050_BYTES_PER_SAMPLE;

            if (sample_count == 0) return ESP_OK;

            if (sample_count > MPU6050_MAX_BATCH_SAMPLES) sample_count = MPU6050_MAX_BATCH_SAMPLES;

            ret = device_read_multiple_fifo_samples(sample_count, samples);
            if (ret != ESP_OK) return ret;

            ESP_LOGD("MPU6050", "Read %u samples", (unsigned)sample_count);
        }
    }

static void acquisition_task(void *arg)
    {
        while (1) {

            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);  // wait for interrupt here

            esp_err_t ret = check_and_reset_FIFO_overflow();

            if (ret != ESP_OK) {
            ESP_LOGE("MPU6050", "FIFO overflow check/recovery failed");
            continue; // back to waiting for interrupt
            }

            ret = drain_fifo();

            if (ret != ESP_OK) {
            ESP_LOGE("MPU6050", "FIFO drain failed");
            }
        }
    }

esp_err_t device_start_acquisition_task(void)
    {
        if (acquisition_task_handle != NULL) {
        return ESP_ERR_INVALID_STATE;
        }

        BaseType_t ret = xTaskCreate(
        acquisition_task,          // Function to execute
        "mpu_acquisition",         // Task name
        4096,                      // maximum stack memory allocated to the task
        NULL,                      // Task argument
        5,                         // Priority
        &acquisition_task_handle
        );

        return ret == pdPASS ? ESP_OK : ESP_FAIL;
    }

/* Test-only helpers to access acquisition_task_handle to control (suspend/resume) acquisition_task */

esp_err_t device_suspend_acquisition(void)
    {
        if (acquisition_task_handle == NULL) {
        return ESP_ERR_INVALID_STATE;
        }

        vTaskSuspend(acquisition_task_handle);
        return ESP_OK;
    }

esp_err_t device_resume_acquisition(void)
    {

        if (acquisition_task_handle == NULL) {
        return ESP_ERR_INVALID_STATE;
        }

        vTaskResume(acquisition_task_handle);

        return ESP_OK;
    }


/* Private register read/write helper functions*/

static esp_err_t register_write(  // Write one byte to a register.
    uint8_t register_address,
    uint8_t value)
    {
        if (MPU6050_device_handle == NULL) {
        return ESP_ERR_INVALID_STATE;
        }

        const uint8_t write_buffer[2] = {
        register_address,
        value
        };

        return i2c_master_transmit(
        MPU6050_device_handle,
        write_buffer,
        sizeof(write_buffer),
        1000);
    }

static esp_err_t register_read( // Read one or more consecutive register bytes.
    uint8_t register_address,
    uint8_t *buffer,
    size_t length)
    {
        if (MPU6050_device_handle == NULL) {
        return ESP_ERR_INVALID_STATE;
        }

        if (buffer == NULL || length == 0) {
        return ESP_ERR_INVALID_ARG;
        }

        return i2c_master_transmit_receive(
        MPU6050_device_handle,
        &register_address,
        sizeof(register_address),
        buffer,
        length,
        1000);
    }

esp_err_t access_register_read( // public helper for testing functionality
    uint8_t register_address,
    uint8_t *buffer,
    size_t length)
    {
        return register_read(
        register_address,
        buffer,
        length);
    }

esp_err_t bus_init(void)
    {
        return i2c_new_master_bus(&master_bus_config, &master_bus_handle);
    }


esp_err_t device_init(void)
    {
        if (master_bus_handle == NULL) {
        return ESP_ERR_INVALID_STATE;
        }

        return i2c_master_bus_add_device(
        master_bus_handle,
        &device_config,
        &MPU6050_device_handle);
    }

esp_err_t device_read_id(void)
    {

        uint8_t data_buffer;

        esp_err_t ret = register_read(
        WHO_AM_I,
        &data_buffer,
        sizeof(data_buffer));

        if (ret != ESP_OK) {
        return ret;
        }

        if (data_buffer != MPU6050_ID) {
        ESP_LOGE("MPU6050",
        "Unexpected device ID: 0x%02X",
        (unsigned)data_buffer);
        return ESP_ERR_INVALID_RESPONSE;
        }

        ESP_LOGI("MPU6050", "Device ID verified: 0x%02X",
        (unsigned)data_buffer);

        return ESP_OK;

    }

esp_err_t device_wake(void)
    {
        uint8_t data_buffer;

        esp_err_t ret = register_read(
        PWR_MGMT_1,
        &data_buffer,
        sizeof(data_buffer));

        if (ret != ESP_OK) {
        return ret;
        }

        uint8_t result = data_buffer & ~MPU6050_SLEEP_BIT;

        ret = register_write(PWR_MGMT_1, result); // Clear sleep bit
        if (ret != ESP_OK) {
        return ret;
        }

        ret = register_read(
        PWR_MGMT_1,
        &data_buffer,
        sizeof(data_buffer));
        if (ret != ESP_OK) return ret;

        if (data_buffer & MPU6050_SLEEP_BIT) {  // Check sleep bit
        printf("Sensor waking error\n");
        return ESP_FAIL;
    }

    return ESP_OK;

    }

esp_err_t device_configure_sampling(void)
    {
        /*
        CONFIG sets and configures the filter bandwidth

        Note that bandwidth should be < fs/2 to reduce aliasing
        ie the highest frequency you can sample should not be aliased

        NB I2C transfer speed and sensor sample rate are two independent settings

        Sensor produces bytes at MPU6050_BYTES_PER_SAMPLE bytes per cycle (X = 2, Y = 2, Z = 2)
        Sensor cycles at fs per second
        So eg sensor produces 600 bytes per second into FIFO

        Hence FIFO will fill in 1024/600 = 1.7 seconds approx

        Hence we have (each byte is accompanied by ack/nack bit):

        I2C byte transfer rate out of FIFO = 100kHz/9 bits = 11 kbytes/second approx

        NB the additional I2C overhead will reduce this further

        device_configure_sampling enables and configures low pass filter
        For example:
        0x02 = 94 Hz
        0x03 = 44 Hz
        0x04 = 21 Hz
        0x06 = 5 Hz
        */
        esp_err_t ret = register_write(CONFIG, 0x03);
        if (ret != ESP_OK) {
        return ret;
        }

        return register_write(SMPLRT_DIV, 0x09);  //Set the sampling frequency fs, to 100 Hz via fs = 1000/(1+SMPLRT_DIV)
    }

esp_err_t device_read_device_sensitivity_and_range(unsigned *range, unsigned *sensitivity)
    {
        if (sensitivity == NULL || range == NULL ) {
        return ESP_ERR_INVALID_ARG;
        }

        /*
        First extract and process AFS_SEL (bits 4 and 3)
        */

        uint8_t data;

        esp_err_t ret = register_read(ACCEL_CONFIG, &data, 1);
        if (ret != ESP_OK) return ret;

        uint8_t afs_sel = (data >> 3) & 0x03;

        /* Secondly obtain the range */

        const unsigned range_table[4] = {
        2, 4, 8, 16
        };

        *range = range_table[afs_sel];

        /* Thirdly obtain the corresponding sensitivity */

        const unsigned sensitivity_table[4] = { // Sensitivity in LSB/g for ±2g, ±4g, ±8g, ±16g
        16384, 8192, 4096, 2048
        };

        *sensitivity = sensitivity_table[afs_sel];

        return ESP_OK;

    }

esp_err_t device_enable_fifo(void) //starts data streaming into fifo as soon as it is called
    {
        esp_err_t ret = register_write(FIFO_EN, 0x00);  // Disable FIFO
        if (ret != ESP_OK) return ret;

        ret = register_write(USER_CTRL, 0x00);  // Disable FIFO
        if (ret != ESP_OK) return ret;

        ret = register_write(USER_CTRL, MPU6050_FIFO_RESET); // Reset FIFO
        if (ret != ESP_OK) return ret;

        ret = register_write(USER_CTRL, MPU6050_FIFO_ENABLE);  //enable FIFO hardware
        if (ret != ESP_OK) return ret;

        return register_write(FIFO_EN, MPU6050_ACCEL_FIFO_ENABLE); // Store accelerometer XYZ samples only, ie we are setting bit ACCEL_FIFO_EN
    }

esp_err_t device_count_fifo_bytes(uint16_t *byte_count)
    {
        if (byte_count == NULL) {
        return ESP_ERR_INVALID_ARG;
        }

        uint8_t data[2];

        esp_err_t ret = register_read(FIFO_COUNTH, data, sizeof(data));     // Read FIFO_COUNTH and FIFO_COUNTL in one transaction
        if (ret != ESP_OK) {
        return ret;
        }

        *byte_count = ((uint16_t)data[0] << 8) | data[1];

        return ESP_OK;
    }

esp_err_t device_read_multiple_fifo_samples(size_t sample_count, accel_3_axis_sample *sample) //we will use count and modify sample for the caller
    {
        if (sample == NULL || sample_count == 0 || sample_count > MPU6050_MAX_BATCH_SAMPLES) {
        return ESP_ERR_INVALID_ARG;
        }

        uint8_t data[MPU6050_MAX_BATCH_SAMPLES * MPU6050_BYTES_PER_SAMPLE];

        esp_err_t ret = register_read(FIFO_R_W, data, sample_count * MPU6050_BYTES_PER_SAMPLE);  //Read only the requested bytes
        if (ret != ESP_OK) {
        return ret;
        }

        for (size_t j = 0; j < sample_count; j++) {
            for (int i = 0; i < 3; i++) {
                sample[j].axis_reading[i] =
                (int16_t)((data[j * MPU6050_BYTES_PER_SAMPLE + 2*i] << 8) |
                data[j * MPU6050_BYTES_PER_SAMPLE + 2*i + 1]);
              }
        }
        return ESP_OK;

    }

esp_err_t check_and_reset_FIFO_overflow(void)
    {
        uint8_t int_status;

        esp_err_t ret = register_read(INT_STATUS, &int_status, 1);
        if (ret != ESP_OK) return ret;

        if ((int_status & 0x10) != 0) {
        ESP_LOGW("MPU6050", "Overflow detected");

        ret = device_enable_fifo();
        if (ret != ESP_OK) return ret;

        ESP_LOGI("MPU6050", "Overflow reset complete");
        }

        // need to add a check to this function to make sure FIFO reset completes fully before further reads can take place - TO DO
        // else FIFO may be left in a non-defined state when eg acquisition_task() calls it

        return ESP_OK;
    }

esp_err_t device_enable_interrupt(void) // Here we configure the interrupts sent by MPU6050. NB MPU6050 supports several interrupt sources
    {
        esp_err_t ret = register_write(INT_PIN_CFG, 0x00);
        if (ret != ESP_OK) return ret;
        /*ie all INT_PIN_CFG register bits unset. hence we have:

        The logic level for the INT pin is active high ie MPU6050 will set in interrupt pin high to send an interrupt
        Push-pull output: The sensor actively drives the pin HIGH or LOW.
        Pulse mode: The interrupt signal is a short pulse rather than remaining HIGH.
        Clear on status-register read: Reading INT_STATUS clears the interrupt status
        Next 2 bits deal with the FSYNC pin, which we set as disabled
        (used to synchronise sensor measurements with an external signal.)
        Finally bit1 disables access auxillary bus (used for additional sensors)
        Bit0 is reserved so left at 0

        Following has been set:
        Active high: INT goes HIGH when triggered.
        Push-pull output: The sensor actively drives the pin HIGH or LOW.
        Pulse mode: The interrupt signal is a short pulse rather than remaining HIGH.
        Clear on status-register read: Reading INT_STATUS clears the interrupt status.
        */

        ret = register_write(INT_ENABLE, 0x01); // data ready interrupt
        if (ret != ESP_OK) return ret;

        //Verify that interrupt is enabled
        uint8_t int_enable;

        ret = register_read(INT_ENABLE , &int_enable, 1);
        if (ret != ESP_OK) return ret;

        if ((int_enable & 0x01) == 0) {
        return ESP_FAIL;
        }

        return ESP_OK;

    }

static void IRAM_ATTR mpu6050_isr(void *arg) // Interrupt Service Routine (ISR)
    {
        if (acquisition_task_handle == NULL) return; // prevents an unexpected early GPIO interrupt from notifying a null task handle.

        BaseType_t higher_priority_woken = pdFALSE;

        vTaskNotifyGiveFromISR(
        acquisition_task_handle,
        &higher_priority_woken
        );

        portYIELD_FROM_ISR(higher_priority_woken);
    }

esp_err_t configure_interrupt_gpio(void) // Configure  esp32 pin GPIO27 to detect the MPU6050's interrupt signal
    {
        if (interrupt_gpio_configured) {
        return ESP_ERR_INVALID_STATE;
        }

        gpio_config_t config = {
        .pin_bit_mask = (1ULL << MPU_INT_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_POSEDGE
        };

        esp_err_t ret = gpio_config(&config); //Configure GPIO27
        if (ret != ESP_OK) return ret;

        ret = gpio_install_isr_service(0); // Install GPIO interrupt service
        if (ret != ESP_OK) return ret;

        ret = gpio_isr_handler_add(MPU_INT_GPIO, mpu6050_isr, NULL);  //Register the ISR which esp32 will run each time interrupt is registered
        if (ret != ESP_OK) return ret;

        interrupt_gpio_configured = true;

        return ESP_OK;
    }
