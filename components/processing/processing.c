
// processing.c







esp_err_t device_read_acc_range(unsigned *sensitivity)    // AFS_SEL selects a full scale range which has a corresponding sensitivity
    {
    if (sensitivity == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t config;

    //AFS_SEL is bits 4–3 of ACCEL_CONFIG
    //The default setting is AFS_SEL = 0, meaning ±2 g and 16,384 raw counts per g.

    esp_err_t ret = register_read(ACCEL_CONFIG, &config, sizeof(config));
    if (ret != ESP_OK) {
        return ret;
        }

    /*
    This extracts bits 4–3 from config
    1. config >> 3 shifts them into positions 1–0.
    2. & 0x03u masks with binary 00000011, keeping only those two bits.
    3. The result, afs_sel, is therefore 0, 1, 2 or 3
    4. ie it makes for easy indexing the table on page 15 (for range) and page 29 (for sensitivity) of the register map document
    */

    uint8_t afs_sel = (config >> 3) & 0x03u;

    static const unsigned sensitivities[4] = {  ////Your sensitivity values include 16,384, so they won’t fit in uint8_t. so we use 4 byte unsigned int
    16384, 8192, 4096, 2048
    };

    *sensitivity = sensitivities[afs_sel];  //yep ok just assign now caller can pass a sensitivity and it will be populated

    //So basically we are extracting AFS_SEL in a form that allows us to read the corresponding sensitivity
    //ie isolating the last two bits, then converting their result into decimal
    //so eg if bit0 = bit1 = 1, Hence = AFS_SEL = 3 which in the table gives ± 16g
    //This sensitivity has a range associated with it

    return ESP_OK;
    }









esp_err_t device_read_acc_x_axis(unsigned sensitivity)
    {
    if (sensitivity == 0) {
        return ESP_ERR_INVALID_ARG;
        }

    uint8_t data[2];

    esp_err_t ret = register_read(      // Read X-axis acceleration: two bytes.
    ACCEL_XOUT_H,
    data,
    sizeof(data));

    if (ret != ESP_OK) {
        return ret;
        }

    //now received bytes still need combining into a signed 16-bit reading
    //shift the high (first rec) byte into the upper 8 positions to make room for the second byte
    //OR the reseult to add together

    int16_t acc_reading = (data[0] << 8) | data[1];

    float acceleration_g = calc_acc_from_raw(acc_reading, sensitivity); //Helper function

    printf("X-axis: raw=%d, acceleration=%.4f g\n",
    acc_reading, acceleration_g);

    return ESP_OK;
    }





esp_err_t device_read_acc_3_axis(unsigned sensitivity,accel_3_axis_sample *sample )      //we will use sensitivity and modify sample

    {

    if (sensitivity == 0 || sample == NULL ) {  //
        return ESP_ERR_INVALID_ARG;
        }

    uint8_t data[6];  //for all 6 bytes ie all 3 acc axes

    char axes_array[] = {'X', 'Y', 'Z'};

    esp_err_t ret = register_read(    //one burst read
        ACCEL_XOUT_H,
        data,
        sizeof(data));

    if (ret != ESP_OK) {
        return ret;
        }

    //ok now all 6 bytes have been read

    //now populate sample in a loop
    for (int i = 0; i < 3; i++) {

        int16_t acc_reading = (data[2*i] << 8) | data[2*i + 1]; //need to advance 2 bytes per loop call

        sample->axis_reading[i] = acc_reading;  //The -> operator accesses a struct member through a pointer. , use -> because sample is now a pointer passed by the caller

        printf("%c-axis: raw=%d \n",
        axes_array[i], acc_reading);

        }
        return ESP_OK;

    }






// Helper function to calculate acceleration in g from raw reading
static float calc_acc_from_raw(int16_t raw_reading, unsigned sensitivity) //does not modify either and they are both small so pass by value

    {
    ////Acceleration in g = raw reading ÷ sensitivity. For ±2 g, sensitivity is 16,384 counts/g.
    float acceleration_g = (float)raw_reading / sensitivity;

    return acceleration_g;

    }









/* Example use - Obtain sensitivity for subsequent calculation of g */

uint16_t sensitivity;

ret = device_read_acc_range(&sensitivity);
if (ret != ESP_OK) {
    return ret;
