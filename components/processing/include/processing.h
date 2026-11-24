
// processing.h

// Converting raw readings to g, calculating statistics and preparing data for telemetry.

esp_err_t device_read_acc_range(unsigned *sensitivity); //obtain the full scale range and sensitivity in order to use the result to convert readings to g
esp_err_t device_read_acc_x_axis(unsigned sensitivity);
esp_err_t device_read_acc_3_axis(unsigned sensitivity,accel_3_axis_sample *sample );
