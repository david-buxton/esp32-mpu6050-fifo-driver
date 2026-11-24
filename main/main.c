
// main.c

#include "acc_driver.h"
#include "esp_log.h"
#include "test_acquisition.h"

static const char *TAG = "MPU6050";

#if ACTIVE_TEST == TEST_NONE || ACTIVE_TEST == TEST_FIFO_OVERFLOW

static esp_err_t device_interrupt_init(
    void) // configure GPIO interrupts, create the acquisition task and enable MPU6050 interrupts.
{

    /*  Enable interrupts   */

    esp_err_t ret = configure_interrupt_gpio();
    if (ret != ESP_OK)
        return ret;

    ret = device_start_acquisition_task();
    if (ret != ESP_OK)
        return ret;

    ret = device_enable_interrupt();
    if (ret != ESP_OK)
        return ret;

    return ESP_OK;
}

#endif

static esp_err_t app_driver_init(void) // Initialise and configure the I2C interface and hardware
{

    esp_err_t ret = bus_init();
    if (ret != ESP_OK)
        return ret;

    ret = device_init();
    if (ret != ESP_OK)
        return ret;

    ret = device_read_id();
    if (ret != ESP_OK)
        return ret;

    ret = device_wake();
    if (ret != ESP_OK)
        return ret;

    ret = device_configure_sampling();
    if (ret != ESP_OK)
        return ret;

    return ESP_OK;
}

void app_main(void)
{
    ESP_LOGI(TAG, "Initialisation");
    ESP_ERROR_CHECK(app_driver_init());

#if ACTIVE_TEST == TEST_ACCEL_READING
    ESP_ERROR_CHECK(accel_reading_test());

#elif ACTIVE_TEST == TEST_FIFO_ACCUMULATION
    ESP_ERROR_CHECK(fifo_accumulation_count_test()); // acquisition task must not be active for this test

#elif ACTIVE_TEST == TEST_FIFO_DRAIN
    ESP_ERROR_CHECK(fifo_drain_test());

#elif ACTIVE_TEST == TEST_FIFO_OVERFLOW
    ESP_ERROR_CHECK(device_enable_fifo());
    ESP_ERROR_CHECK(device_interrupt_init());
    ESP_ERROR_CHECK(fifo_overflow_test());

#elif ACTIVE_TEST == TEST_NONE
    ESP_ERROR_CHECK(device_enable_fifo());
    ESP_ERROR_CHECK(device_interrupt_init());

#else
#error "Invalid test selection"
#endif
}
