#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "cj202_co2_sensor.h"
#include "cj202_internal.h"

static const char *TAG = "CJ202_GPIO";

// Interrupt service routine
static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    cj202_dev_t *dev = (cj202_dev_t *)arg;
    uint64_t current_time = esp_timer_get_time() / 1000; // Convert to milliseconds
    int gpio_level = gpio_get_level(dev->gpio_num);
    uint8_t gpio_num = dev->gpio_num;
    
    if (gpio_level == 1) {
        // Rising edge
        dev->rising_time = current_time;
        if (dev->falling_time > 0) {
            // Calculate period time
            dev->period_time_ms = current_time - dev->falling_time + dev->high_level_time_ms;
        }
    } else {
        // Falling edge
        dev->falling_time = current_time;
        if (dev->rising_time > 0) {
            // Calculate high level time
            dev->high_level_time_ms = current_time - dev->rising_time;
            dev->measurement_ready = true;
        }
    }
    
    xQueueSendFromISR(dev->gpio_evt_queue, &gpio_num, NULL);
}

// GPIO task
static void cj202_gpio_task(void* arg)
{
    cj202_dev_t *dev = (cj202_dev_t *)arg;
    uint32_t io_num;
    
    ESP_LOGI(TAG, "CJ202 GPIO task started");
    
    while (1) {
        if (xQueueReceive(dev->gpio_evt_queue, &io_num, portMAX_DELAY)) {
            if (dev->measurement_ready) {
                // Verify period is within expected range (1004ms ±5%)
                if (dev->period_time_ms >= 950 && dev->period_time_ms <= 1050) {
                    dev->co2_ppm = cj202_calculate_co2_ppm(dev->high_level_time_ms, dev->period_time_ms);
                }
                dev->measurement_ready = false;
            }
        }
    }
}

// Initialize CO2 sensor with GPIO method
esp_err_t cj202_gpio_init(cj202_dev_t *dev)
{
    if (dev == NULL) {
        ESP_LOGE(TAG, "Device handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Initialize device state
    dev->rising_time = 0;
    dev->falling_time = 0;
    dev->measurement_ready = false;
    dev->high_level_time_ms = 0;
    dev->period_time_ms = 0;
    dev->co2_ppm = 0;
    
    // Create queue to handle gpio events
    dev->gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    if (dev->gpio_evt_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create queue");
        return ESP_FAIL;
    }
    
    // Configure GPIO
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_ANYEDGE,   // Trigger on both rising and falling edges
        .mode = GPIO_MODE_INPUT,          // Input mode
        .pin_bit_mask = (1ULL << dev->gpio_num),
        .pull_down_en = 0,
        .pull_up_en = 0,
    };
    
    // Apply GPIO configuration
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "GPIO config failed");
        vQueueDelete(dev->gpio_evt_queue);
        dev->gpio_evt_queue = NULL;
        return ret;
    }
    
    // Install GPIO ISR service
    ret = gpio_install_isr_service(dev->intr_alloc_flags);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        // ESP_ERR_INVALID_STATE means ISR service is already installed, not an error
        ESP_LOGE(TAG, "ISR service install failed");
        vQueueDelete(dev->gpio_evt_queue);
        dev->gpio_evt_queue = NULL;
        return ret;
    }
    
    // Add GPIO interrupt handler
    ret = gpio_isr_handler_add(dev->gpio_num, gpio_isr_handler, dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ISR handler add failed");
        vQueueDelete(dev->gpio_evt_queue);
        dev->gpio_evt_queue = NULL;
        return ret;
    }
    
    // Create task to process GPIO events
    BaseType_t task_ret = xTaskCreate(cj202_gpio_task, "cj202_gpio_task", 3072, dev, 10, &dev->gpio_task_handle);
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Task creation failed");
        gpio_isr_handler_remove(dev->gpio_num);
        vQueueDelete(dev->gpio_evt_queue);
        dev->gpio_evt_queue = NULL;
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "CJ202 CO2 sensor initialized, using GPIO pin: %d", dev->gpio_num);
    return ESP_OK;
}

// Get current CO2 concentration (GPIO mode)
uint32_t cj202_gpio_get_ppm(cj202_dev_t *dev)
{
    if (dev == NULL) {
        ESP_LOGE(TAG, "Device handle is NULL");
        return 0;
    }
    
    return dev->co2_ppm;
}

// Deinitialize CO2 sensor (GPIO mode)
esp_err_t cj202_gpio_deinit(cj202_dev_t *dev)
{
    if (dev == NULL) {
        ESP_LOGE(TAG, "Device handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Remove GPIO ISR handler
    esp_err_t ret = gpio_isr_handler_remove(dev->gpio_num);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to remove ISR handler");
    }
    
    // Delete task if it exists
    if (dev->gpio_task_handle != NULL) {
        vTaskDelete(dev->gpio_task_handle);
        dev->gpio_task_handle = NULL;
    }
    
    // Delete queue if it exists
    if (dev->gpio_evt_queue != NULL) {
        vQueueDelete(dev->gpio_evt_queue);
        dev->gpio_evt_queue = NULL;
    }
    
    ESP_LOGI(TAG, "CJ202 CO2 sensor deinitialized (GPIO mode)");
    return ESP_OK;
} 