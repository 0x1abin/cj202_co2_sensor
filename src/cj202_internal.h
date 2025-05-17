#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "cj202_co2_sensor.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief CJ202 CO2 sensor device structure
 */
typedef struct cj202_dev_t {
    uint8_t gpio_num;                  /*!< GPIO pin number */
    cj202_capture_mode_t mode;         /*!< Capture mode */
    uint32_t co2_ppm;                  /*!< Current CO2 concentration in ppm */
    int intr_alloc_flags;              /*!< Optional Interrupt allocation flags */
    
    // GPIO specific data
    QueueHandle_t gpio_evt_queue;      /*!< GPIO event queue */
    TaskHandle_t gpio_task_handle;     /*!< GPIO task handle */
    uint64_t rising_time;              /*!< Rising edge timestamp (ms) */
    uint64_t falling_time;             /*!< Falling edge timestamp (ms) */
    bool measurement_ready;            /*!< Flag indicating if measurement is ready */
    uint32_t high_level_time_ms;       /*!< High level time in milliseconds */
    uint32_t period_time_ms;           /*!< Period time in milliseconds */
    
#if !defined(CONFIG_IDF_TARGET_ESP32C2) && !defined(CONFIG_IDF_TARGET_ESP32C3)
    // MCPWM specific data
    TaskHandle_t mcpwm_task_handle;    /*!< MCPWM task handle */
    void *cap_timer;                   /*!< MCPWM capture timer handle */
    void *cap_chan;                    /*!< MCPWM capture channel handle */
    uint32_t prev_high_ticks;          /*!< Previous high level ticks */
    uint32_t prev_period_ticks;        /*!< Previous period ticks */
    bool first_measurement;            /*!< Flag for first measurement */
    uint32_t last_capture_time;        /*!< Last capture timestamp */
#endif
} cj202_dev_t;

/**
 * @brief Initialize CO2 sensor with GPIO method
 * 
 * @param dev Device handle
 * @return esp_err_t ESP_OK: success, others: failed
 */
esp_err_t cj202_gpio_init(cj202_dev_t *dev);

/**
 * @brief Get current CO2 concentration (GPIO mode)
 * 
 * @param dev Device handle 
 * @return uint32_t Current CO2 concentration in ppm (0-5000)
 */
uint32_t cj202_gpio_get_ppm(cj202_dev_t *dev);

/**
 * @brief Deinitialize CO2 sensor (GPIO mode)
 * 
 * @param dev Device handle
 * @return esp_err_t ESP_OK: success, others: failed
 */
esp_err_t cj202_gpio_deinit(cj202_dev_t *dev);

#if !defined(CONFIG_IDF_TARGET_ESP32C2) && !defined(CONFIG_IDF_TARGET_ESP32C3)
/**
 * @brief Initialize CO2 sensor with MCPWM method
 * 
 * @param dev Device handle
 * @return esp_err_t ESP_OK: success, others: failed
 */
esp_err_t cj202_mcpwm_init(cj202_dev_t *dev);

/**
 * @brief Get current CO2 concentration (MCPWM mode)
 * 
 * @param dev Device handle
 * @return uint32_t Current CO2 concentration in ppm (0-5000)
 */
uint32_t cj202_mcpwm_get_ppm(cj202_dev_t *dev);

/**
 * @brief Deinitialize CO2 sensor (MCPWM mode)
 * 
 * @param dev Device handle
 * @return esp_err_t ESP_OK: success, others: failed
 */
esp_err_t cj202_mcpwm_deinit(cj202_dev_t *dev);
#endif

/**
 * @brief Calculate CO2 concentration
 * 
 * Formula: Cppm = 5000 × (TH-2ms) / (TH+TL-4ms)
 * 
 * @param high_level_ms High level time (milliseconds)
 * @param period_ms Period time (milliseconds)
 * @return uint32_t CO2 concentration
 */
uint32_t cj202_calculate_co2_ppm(uint32_t high_level_ms, uint32_t period_ms);

#ifdef __cplusplus
}
#endif 