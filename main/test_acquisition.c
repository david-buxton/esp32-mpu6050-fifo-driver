
// test_acquisition.c

#include "test_acquisition.h"
#include "MPU6050_registers.h"
#include "acc_driver.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "TEST";

static esp_err_t device_read_g_acc_3_axis(g_acc_3_axis_sample *sample, unsigned sensitivity)
{

    if (sample == NULL || sensitivity == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t data[MPU6050_BYTES_PER_SAMPLE];

    esp_err_t ret = access_register_read(ACCEL_XOUT_H, data, sizeof(data));
    if (ret != ESP_OK)
        return ret;

    for (int i = 0; i < 3; i++)
    {

        // Combine two bytes into a signed 16-bit integer
        int16_t raw_reading = (int16_t)((data[2 * i] << 8) | data[2 * i + 1]);

        // Convert raw reading into g
        sample->axis_reading[i] = (float)raw_reading / sensitivity; // g = raw reading / sensitivity
    }

    return ESP_OK;
}

esp_err_t accel_reading_test(void)
{

    /*
    Check the reading returns reasonable values with the sensor lying flat. eg:
    X=0.022 g Y=0.003 g Z=0.977 g
    ie Z is approximately 1g, consistent with the sensor lying flat.
    Check X and Y show small variations, as expected from sensor noise.
    Check when board is turned upside down, Z = approx -1g, X and Y similar to before
    */

    /* First report range and sensitivity */

    unsigned device_range;
    unsigned device_sensitivity;

    ESP_ERROR_CHECK(device_read_device_sensitivity_and_range(&device_range, &device_sensitivity));

    ESP_LOGI(TAG, "Accelerometer range: ±%u g", device_range);
    ESP_LOGI(TAG, "Accelerometer sensitivity: %u LSB/g", device_sensitivity);

    /* Then single 3 axis g read */

    g_acc_3_axis_sample reading;

    ESP_ERROR_CHECK(device_read_g_acc_3_axis(&reading, device_sensitivity));

    ESP_LOGI("Test result", "X=%.3f g Y=%.3f g Z=%.3f g", reading.axis_reading[0], reading.axis_reading[1],
             reading.axis_reading[2]);

    return ESP_OK;
}

esp_err_t fifo_accumulation_count_test(void)
{
    uint16_t fifo_count = 0;

    // Assume acquisition task is disabled
    esp_err_t ret = device_enable_fifo();
    if (ret != ESP_OK)
        return ret;

    vTaskDelay(pdMS_TO_TICKS(500));

    ret = device_count_fifo_bytes(&fifo_count);
    if (ret != ESP_OK)
        return ret;

    ESP_LOGI(TAG, "FIFO bytes after 500ms: %u", (unsigned)fifo_count);

    return ESP_OK;
}

esp_err_t fifo_drain_test(void)
{
    uint16_t fifo_count_initial;
    uint16_t fifo_count_final;

    // Reset and enable FIFO
    esp_err_t ret = device_enable_fifo();
    if (ret != ESP_OK)
        return ret;

    // Accumulate approximately 300 bytes at 100 Hz
    vTaskDelay(pdMS_TO_TICKS(500));

    // Read initial FIFO byte count
    ret = device_count_fifo_bytes(&fifo_count_initial);
    if (ret != ESP_OK)
        return ret;

    ESP_LOGI(TAG, "Initial FIFO bytes: %u", (unsigned)fifo_count_initial);

    // Calculate complete sample bytes
    size_t bytes_to_read = fifo_count_initial - (fifo_count_initial % MPU6050_BYTES_PER_SAMPLE);

    if (fifo_count_initial % MPU6050_BYTES_PER_SAMPLE != 0)
    {
        ESP_LOGW(TAG, "Partial sample bytes detected");
    }

    // Allocate large buffer
    uint8_t data[MPU6050_BYTES_PER_SAMPLE * 100];

    if (bytes_to_read == 0 || bytes_to_read > sizeof(data))
    {
        return ESP_FAIL;
    }

    // Drain FIFO in one read
    ret = access_register_read(FIFO_R_W, data, bytes_to_read);
    if (ret != ESP_OK)
        return ret;

    // Read remaining FIFO byte count
    ret = device_count_fifo_bytes(&fifo_count_final);
    if (ret != ESP_OK)
        return ret;

    ESP_LOGI(TAG, "Bytes drained: %u", (unsigned)bytes_to_read);

    ESP_LOGI(TAG, "Remaining FIFO bytes: %u",
             (unsigned)fifo_count_final); // Verify FIFO has largely emptied

    return ESP_OK;
}

esp_err_t fifo_overflow_test(void)
{
    uint16_t fifo_count = 0;

    ESP_LOGI(TAG, "Starting FIFO overflow test");

    vTaskDelay(pdMS_TO_TICKS(500)); // Allow normal acquisition

    ESP_ERROR_CHECK(device_suspend_acquisition());

    ESP_LOGI(TAG, "Acquisition suspended for 3 seconds");

    vTaskDelay(pdMS_TO_TICKS(3000)); // FIFO continues filling while task is suspended

    // report bytes in fifo
    esp_err_t ret = device_count_fifo_bytes(&fifo_count);

    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "FIFO contains %u bytes",
                 (unsigned)fifo_count); // Expected result: Approximately 1024 bytes after 3 seconds, since the FIFO
                                        // fills in roughly 1.7 seconds at 100 Hz.
    }
    else
    {
        ESP_LOGE(TAG, "Failed to read FIFO count");
    }

    ESP_ERROR_CHECK(device_resume_acquisition());

    return ret; // NB this test doesn't automatically verify success
}
