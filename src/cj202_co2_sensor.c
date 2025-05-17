#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "esp_log.h"
#include "cj202_co2_sensor.h"
#include "cj202_internal.h"

static const char *TAG = "CJ202";

esp_err_t cj202_init(const cj202_config_t *config, cj202_handle_t *handle)
{
    if (config == NULL || handle == NULL) {
        ESP_LOGE(TAG, "Config or handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    // Allocate memory for device handle
    cj202_dev_t *dev = calloc(1, sizeof(cj202_dev_t));
    if (dev == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for device");
        return ESP_ERR_NO_MEM;
    }

    // Copy configuration
    dev->gpio_num = config->gpio_num;
    dev->mode = config->mode;
    dev->intr_alloc_flags = config->intr_alloc_flags;
    dev->co2_ppm = 0;

    ESP_LOGI(TAG, "Initializing CJ202 CO2 sensor, mode: %d, GPIO: %d", dev->mode, dev->gpio_num);

    esp_err_t ret = ESP_OK;
    
    // Initialize based on capture mode
    switch (dev->mode) {
        case CJ202_MODE_GPIO_INTERRUPT:
            ret = cj202_gpio_init(dev);
            break;
#if !defined(CONFIG_IDF_TARGET_ESP32C2) && !defined(CONFIG_IDF_TARGET_ESP32C3)
        case CJ202_MODE_MCPWM_CAPTURE:
            ret = cj202_mcpwm_init(dev);
            break;
#endif
        default:
            ESP_LOGE(TAG, "Unsupported mode: %d", dev->mode);
            ret = ESP_ERR_NOT_SUPPORTED;
            break;
    }

    if (ret != ESP_OK) {
        free(dev);
        return ret;
    }

    // Set handle
    *handle = dev;
    return ESP_OK;
}

uint32_t cj202_get_ppm(cj202_handle_t handle)
{
    if (handle == NULL) {
        ESP_LOGE(TAG, "Handle is NULL");
        return 0;
    }

    cj202_dev_t *dev = (cj202_dev_t *)handle;

    switch (dev->mode) {
        case CJ202_MODE_GPIO_INTERRUPT:
            return cj202_gpio_get_ppm(dev);
#if !defined(CONFIG_IDF_TARGET_ESP32C2) && !defined(CONFIG_IDF_TARGET_ESP32C3)
        case CJ202_MODE_MCPWM_CAPTURE:
            return cj202_mcpwm_get_ppm(dev);
#endif
        default:
            ESP_LOGW(TAG, "Unknown mode, returning 0");
            return 0;
    }
}

esp_err_t cj202_deinit(cj202_handle_t handle)
{
    if (handle == NULL) {
        ESP_LOGE(TAG, "Handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    cj202_dev_t *dev = (cj202_dev_t *)handle;
    esp_err_t ret = ESP_OK;

    // Deinitialize based on capture mode
    switch (dev->mode) {
        case CJ202_MODE_GPIO_INTERRUPT:
            ret = cj202_gpio_deinit(dev);
            break;
#if !defined(CONFIG_IDF_TARGET_ESP32C2) && !defined(CONFIG_IDF_TARGET_ESP32C3)
        case CJ202_MODE_MCPWM_CAPTURE:
            ret = cj202_mcpwm_deinit(dev);
            break;
#endif
        default:
            ESP_LOGW(TAG, "Unknown mode");
            break;
    }

    // Free device memory
    free(dev);
    return ret;
} 