
// test_acquisition.h

#ifndef TEST_ACQUISITION
#define TEST_ACQUISITION

#ifdef __cplusplus
extern "C"
{
#endif

#include "acc_driver.h"
#include "esp_err.h"
#include <stddef.h>
#include <stdint.h>

#define TEST_NONE 0
#define TEST_ACCEL_READING 1
#define TEST_FIFO_ACCUMULATION 2
#define TEST_FIFO_DRAIN 3
#define TEST_FIFO_OVERFLOW 4

//#define ACTIVE_TEST TEST_ACCEL_READING // Test 1 — Accelerometer reading
//#define ACTIVE_TEST TEST_FIFO_ACCUMULATION  // Test 2 — FIFO accumulation
//#define ACTIVE_TEST TEST_FIFO_DRAIN // Test 3 — FIFO drain
//#define ACTIVE_TEST TEST_FIFO_OVERFLOW  // Test 4 — FIFO overflow and recovery
#define ACTIVE_TEST TEST_NONE // Normal operation (no tests)

esp_err_t accel_reading_test(void);

esp_err_t fifo_accumulation_count_test(void);
/*
For fifo_accumulation_count_test test, in contrast to the test below,
I will read FIFO_COUNT rather than the actual data as this approach
isolates the FIFO functionality itself from reading it

Delay reading for 500 ms. At 100 Hz check FIFO filled with approximately 300 bytes
ie 100 Hz × 0.5 seconds × 6 bytes = 300 bytes
*/

esp_err_t fifo_drain_test(void);
/*
Commence FIFO filling
Record number of bytes in FIFO via FIFO_R_W
Read that many bytes out of FIFO (should be no partial bytes, MPU6050 writes complete 6-byte XYZ samples into FIFO)
Check all the bytes were read correctly
NB interrupts are disabled here else acquisition task will compete with fifo drain test

Could also then do a further read of FIFO to check bytes remaining are
consistent with time taken for the drain test
*/

esp_err_t fifo_overflow_test(void);
/*
fifo_overflow_test: samples generated >1024 bytes (~1.7 seconds at 100 Hz).
Confirm overflow detection and recovery.
*/

#ifdef __cplusplus
}
#endif

#endif
