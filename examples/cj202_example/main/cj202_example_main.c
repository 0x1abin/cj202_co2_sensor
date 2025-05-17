#include <stdio.h>
#include <inttypes.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "cj202_co2_sensor.h"

static const char *TAG = "CJ202_EXAMPLE";

cj202_handle_t sensor = NULL;

void app_main(void)
{
    ESP_LOGI(TAG, "CJ202 CO2 sensor example starting");

    cj202_config_t config = CJ202_DEFAULT_CONFIG();

    // MCPWM capture mode is not supported on ESP32C2 and ESP32C3
    // config.mode = CJ202_MODE_MCPWM_CAPTURE;

    esp_err_t ret = cj202_init(&config, &sensor);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Sensor initialization failed: %d", ret);
        return;
    }
    
    while (1) {
        uint32_t co2_ppm = cj202_get_ppm(sensor);
        ESP_LOGI(TAG, "Sensor CO2: %"PRIu32" ppm", co2_ppm);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    cj202_deinit(sensor);
} 