#include <stdio.h>
#include <inttypes.h>
#include "esp_log.h"
#include "cj202_co2_sensor.h"
#include "cj202_internal.h"

#if !defined(CONFIG_IDF_TARGET_ESP32C2) && !defined(CONFIG_IDF_TARGET_ESP32C3)

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_private/esp_clk.h"
#include "driver/mcpwm_cap.h"
#include "driver/gpio.h"

static const char *TAG = "CJ202_MCPWM";

// CO2 sensor characteristics
#define CO2_SENSOR_PERIOD_MS 1004  // Expected period: 1004ms ±5%
#define CO2_PERIOD_MIN_MS 950      // Minimum valid period (ms)
#define CO2_PERIOD_MAX_MS 1050     // Maximum valid period (ms)
#define CO2_CAPTURE_TIMEOUT_MS 1500 // Timeout for capture notification
#define CO2_TASK_STACK_SIZE 4096   // Task stack size

// Structure to hold capture values
typedef struct {
    uint32_t cap_val_begin;  // Timestamp of positive edge
    uint32_t cap_val_end;    // Timestamp of negative edge
    bool pos_edge_captured;  // Flag indicating if positive edge was captured
} co2_capture_data_t;

static bool co2_sensor_capture_callback(mcpwm_cap_channel_handle_t cap_chan, const mcpwm_capture_event_data_t *edata, void *user_data)
{
    cj202_dev_t *dev = (cj202_dev_t *)user_data;
    static co2_capture_data_t co2_cap_data = {0};
    BaseType_t high_task_wakeup = pdFALSE;
    uint32_t tof_ticks = 0;

    // Edge detection logic
    switch (edata->cap_edge) {
    case MCPWM_CAP_EDGE_POS:
        // Store the timestamp when positive edge is detected
        co2_cap_data.cap_val_begin = edata->cap_value;
        co2_cap_data.pos_edge_captured = true;
        break;
        
    case MCPWM_CAP_EDGE_NEG:
        if (co2_cap_data.pos_edge_captured) {
            // Store the timestamp when negative edge is detected
            co2_cap_data.cap_val_end = edata->cap_value;
            co2_cap_data.pos_edge_captured = false;
            
            // Calculate high pulse width in ticks
            if (co2_cap_data.cap_val_end > co2_cap_data.cap_val_begin) {
                tof_ticks = co2_cap_data.cap_val_end - co2_cap_data.cap_val_begin;
                // Notify the task to calculate the CO2 concentration
                xTaskNotifyFromISR(dev->mcpwm_task_handle, tof_ticks, eSetValueWithOverwrite, &high_task_wakeup);
            }
        }
        break;
        
    default:
        break;
    }

    return high_task_wakeup == pdTRUE;
}

static void cj202_mcpwm_task(void *arg)
{
    cj202_dev_t *dev = (cj202_dev_t *)arg;
    uint32_t high_pulse_ticks;
    float high_pulse_ms, period_ms;
    const uint32_t apb_freq = esp_clk_apb_freq();
    const float tick_to_us = 1000000.0f / apb_freq;

    ESP_LOGI(TAG, "CJ202 MCPWM capture task starting");
    
    while (1) {
        // Wait for notification from ISR with high pulse width in ticks
        if (xTaskNotifyWait(0x00, ULONG_MAX, &high_pulse_ticks, pdMS_TO_TICKS(CO2_CAPTURE_TIMEOUT_MS)) == pdTRUE) {
            uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
            uint32_t period_ticks = 0;
            
            // Calculate period if this is not the first measurement
            if (!dev->first_measurement) {
                // Calculate period in ms, convert to ticks
                period_ticks = (current_time - dev->last_capture_time) * (apb_freq / 1000);
            } else {
                dev->first_measurement = false;
            }
            
            dev->last_capture_time = current_time;
            
            // Convert ticks to microseconds and then to milliseconds
            high_pulse_ms = (high_pulse_ticks * tick_to_us) / 1000.0f;
            period_ms = (period_ticks * tick_to_us) / 1000.0f;
            
            // Sanity check on measurements
            if (period_ms > 0 && period_ms >= high_pulse_ms && 
                period_ms >= CO2_PERIOD_MIN_MS && period_ms <= CO2_PERIOD_MAX_MS) {
                
                // Calculate CO2 ppm value
                dev->co2_ppm = cj202_calculate_co2_ppm(high_pulse_ms, period_ms);
                
                // Save for next validation
                dev->prev_high_ticks = high_pulse_ticks;
                dev->prev_period_ticks = period_ticks;
            } else if (dev->prev_period_ticks > 0) {
                // Use previous valid values if current measurement is invalid
                high_pulse_ms = (dev->prev_high_ticks * tick_to_us) / 1000.0f;
                period_ms = (dev->prev_period_ticks * tick_to_us) / 1000.0f;
                
                dev->co2_ppm = cj202_calculate_co2_ppm(high_pulse_ms, period_ms);
                
                ESP_LOGW(TAG, "Using previous valid measurement: CO2: %"PRIu32"ppm", dev->co2_ppm);
            }
        } else {
            ESP_LOGW(TAG, "Timeout waiting for PWM capture");
        }
    }
}

static esp_err_t cleanup_resources(cj202_dev_t *dev, esp_err_t error)
{
    // Cleanup resources in reverse order of creation
    if (dev->cap_chan != NULL) {
        mcpwm_capture_channel_disable(*(mcpwm_cap_channel_handle_t *)&dev->cap_chan);
        mcpwm_del_capture_channel(*(mcpwm_cap_channel_handle_t *)&dev->cap_chan);
        dev->cap_chan = NULL;
    }
    
    if (dev->cap_timer != NULL) {
        mcpwm_capture_timer_disable(*(mcpwm_cap_timer_handle_t *)&dev->cap_timer);
        mcpwm_del_capture_timer(*(mcpwm_cap_timer_handle_t *)&dev->cap_timer);
        dev->cap_timer = NULL;
    }
    
    return error;
}

esp_err_t cj202_mcpwm_init(cj202_dev_t *dev)
{
    esp_err_t ret;
    
    if (dev == NULL) {
        ESP_LOGE(TAG, "Device handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Initialize device state
    dev->co2_ppm = 0;
    dev->prev_high_ticks = 0;
    dev->prev_period_ticks = 0;
    dev->first_measurement = true;
    dev->last_capture_time = 0;
    
    ESP_LOGI(TAG, "Installing capture timer");
    mcpwm_capture_timer_config_t cap_conf = {
        .clk_src = MCPWM_CAPTURE_CLK_SRC_DEFAULT,
        .group_id = 0,
    };
    ret = mcpwm_new_capture_timer(&cap_conf, (mcpwm_cap_timer_handle_t *)&dev->cap_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create capture timer: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Installing capture channel");
    mcpwm_capture_channel_config_t cap_ch_conf = {
        .gpio_num = dev->gpio_num,
        .prescale = 1,
        // Capture on both edges
        .flags.neg_edge = true,
        .flags.pos_edge = true,
        // Pull up internally
        .flags.pull_up = true,
    };
    ret = mcpwm_new_capture_channel(*(mcpwm_cap_timer_handle_t *)&dev->cap_timer, &cap_ch_conf, 
                                   (mcpwm_cap_channel_handle_t *)&dev->cap_chan);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create capture channel: %s", esp_err_to_name(ret));
        return cleanup_resources(dev, ret);
    }

    ESP_LOGI(TAG, "Registering capture callback");
    dev->mcpwm_task_handle = xTaskGetCurrentTaskHandle();
    mcpwm_capture_event_callbacks_t cbs = {
        .on_cap = co2_sensor_capture_callback,
    };
    ret = mcpwm_capture_channel_register_event_callbacks(*(mcpwm_cap_channel_handle_t *)&dev->cap_chan, 
                                                      &cbs, dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register capture callback: %s", esp_err_to_name(ret));
        return cleanup_resources(dev, ret);
    }

    ESP_LOGI(TAG, "Enabling capture channel");
    ret = mcpwm_capture_channel_enable(*(mcpwm_cap_channel_handle_t *)&dev->cap_chan);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable capture channel: %s", esp_err_to_name(ret));
        return cleanup_resources(dev, ret);
    }

    ESP_LOGI(TAG, "Enabling and starting capture timer");
    ret = mcpwm_capture_timer_enable(*(mcpwm_cap_timer_handle_t *)&dev->cap_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable capture timer: %s", esp_err_to_name(ret));
        mcpwm_capture_channel_disable(*(mcpwm_cap_channel_handle_t *)&dev->cap_chan);
        return cleanup_resources(dev, ret);
    }
    
    ret = mcpwm_capture_timer_start(*(mcpwm_cap_timer_handle_t *)&dev->cap_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start capture timer: %s", esp_err_to_name(ret));
        mcpwm_capture_timer_disable(*(mcpwm_cap_timer_handle_t *)&dev->cap_timer);
        mcpwm_capture_channel_disable(*(mcpwm_cap_channel_handle_t *)&dev->cap_chan);
        return cleanup_resources(dev, ret);
    }

    // Create task for processing MCPWM captures
    BaseType_t task_ret = xTaskCreate(cj202_mcpwm_task, "cj202_mcpwm_task", 
                                     CO2_TASK_STACK_SIZE, dev, 10, &dev->mcpwm_task_handle);
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Task creation failed");
        mcpwm_capture_timer_stop(*(mcpwm_cap_timer_handle_t *)&dev->cap_timer);
        return cleanup_resources(dev, ESP_FAIL);
    }
    
    ESP_LOGI(TAG, "CJ202 CO2 sensor initialized (MCPWM mode), using GPIO pin: %d", dev->gpio_num);
    return ESP_OK;
}

uint32_t cj202_mcpwm_get_ppm(cj202_dev_t *dev)
{
    if (dev == NULL) {
        ESP_LOGE(TAG, "Device handle is NULL");
        return 0;
    }
    
    return dev->co2_ppm;
}

esp_err_t cj202_mcpwm_deinit(cj202_dev_t *dev)
{
    if (dev == NULL) {
        ESP_LOGE(TAG, "Device handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Stop capture operations
    if (dev->cap_timer != NULL) {
        mcpwm_capture_timer_stop(*(mcpwm_cap_timer_handle_t *)&dev->cap_timer);
        mcpwm_capture_timer_disable(*(mcpwm_cap_timer_handle_t *)&dev->cap_timer);
    }
    
    if (dev->cap_chan != NULL) {
        mcpwm_capture_channel_disable(*(mcpwm_cap_channel_handle_t *)&dev->cap_chan);
        mcpwm_del_capture_channel(*(mcpwm_cap_channel_handle_t *)&dev->cap_chan);
    }
    
    if (dev->cap_timer != NULL) {
        mcpwm_del_capture_timer(*(mcpwm_cap_timer_handle_t *)&dev->cap_timer);
    }
    
    // Delete task if it exists
    if (dev->mcpwm_task_handle != NULL) {
        vTaskDelete(dev->mcpwm_task_handle);
        dev->mcpwm_task_handle = NULL;
    }
    
    // Clear handles
    dev->cap_chan = NULL;
    dev->cap_timer = NULL;
    
    ESP_LOGI(TAG, "CJ202 CO2 sensor deinitialized (MCPWM mode)");
    return ESP_OK;
}

#endif /* !defined(CONFIG_IDF_TARGET_ESP32C2) && !defined(CONFIG_IDF_TARGET_ESP32C3) */ 