#include <stdio.h>
#include <inttypes.h>
#include "esp_log.h"
#include "cj202_internal.h"

static const char *TAG = "CJ202_COMMON";

// CO2 sensor characteristics
#define CO2_SENSOR_MIN_PPM 0
#define CO2_SENSOR_MAX_PPM 5000

/**
 * @brief Calculate CO2 concentration
 * 
 * Formula: Cppm = 5000 × (TH-2ms) / (TH+TL-4ms)
 * 
 * @param high_level_ms High level time in milliseconds
 * @param period_ms Period time in milliseconds
 * @return uint32_t CO2 concentration
 */
uint32_t cj202_calculate_co2_ppm(uint32_t high_level_ms, uint32_t period_ms)
{
    if (period_ms <= 4 || high_level_ms <= 2) {
        return 0; // Invalid data
    }

    // Apply formula: Cppm = 5000 × (TH-2ms) / (TH+TL-4ms)
    float co2 = 5000.0f * (high_level_ms - 2.0f) / (period_ms - 4.0f);

    // Limit range to 0-5000ppm
    if (co2 < CO2_SENSOR_MIN_PPM) co2 = CO2_SENSOR_MIN_PPM;
    if (co2 > CO2_SENSOR_MAX_PPM) co2 = CO2_SENSOR_MAX_PPM;

    ESP_LOGD(TAG, "Calculate CO2: high_level=%"PRIu32"ms, period=%"PRIu32"ms, CO2=%"PRIu32"ppm", 
             high_level_ms, period_ms, (uint32_t)co2);
    
    return (uint32_t)co2;
} 