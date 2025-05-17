#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief CO2 sensor capture mode
 */
typedef enum {
    CJ202_MODE_GPIO_INTERRUPT, /*!< GPIO interrupt mode */
#if !defined(CONFIG_IDF_TARGET_ESP32C2) && !defined(CONFIG_IDF_TARGET_ESP32C3)
    CJ202_MODE_MCPWM_CAPTURE,  /*!< MCPWM capture mode (not supported on ESP32C2 and ESP32C3) */
#endif
} cj202_capture_mode_t;

/**
 * @brief CJ202 CO2 sensor handle type
 */
typedef struct cj202_dev_t *cj202_handle_t;

/**
 * @brief CJ202 CO2 sensor configuration
 */
typedef struct {
    uint8_t gpio_num;              /*!< GPIO pin number */
    cj202_capture_mode_t mode;     /*!< Capture mode */
    int intr_alloc_flags;          /*!< Interrupt allocation flags */
} cj202_config_t;

/**
 * @brief CJ202 default configuration
 */
#define CJ202_DEFAULT_CONFIG() { \
    .gpio_num = CONFIG_CJ202_DEFAULT_GPIO, \
    .mode = CJ202_MODE_GPIO_INTERRUPT, \
    .intr_alloc_flags = 0, \
}

/**
 * @brief Initialize CJ202 CO2 sensor
 * 
 * @param config Sensor configuration
 * @param handle Pointer to store the sensor handle
 * @return esp_err_t ESP_OK: success, others: failed
 */
esp_err_t cj202_init(const cj202_config_t *config, cj202_handle_t *handle);

/**
 * @brief Get current CO2 concentration
 * 
 * @param handle Sensor handle
 * @return uint32_t Current CO2 concentration in ppm (0-5000)
 */
uint32_t cj202_get_ppm(cj202_handle_t handle);

/**
 * @brief Deinitialize CJ202 CO2 sensor
 * 
 * @param handle Sensor handle to be deinitialized
 * @return esp_err_t ESP_OK: success, others: failed
 */
esp_err_t cj202_deinit(cj202_handle_t handle);

#ifdef __cplusplus
}
#endif 