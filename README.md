# CJ202 CO2 Sensor Component

[中文文档](./README_zh.md)

This component provides functionality for interfacing with the CJ202 CO2 sensor, supporting PWM signal capture and processing.

## Features

- Supports two capture modes:
  - GPIO Interrupt Mode: Compatible with all ESP32 series chips
  - MCPWM Capture Mode: Available on ESP32 series chips that support MCPWM capture (excluding ESP32C2 and ESP32C3)
- Calculates CO2 concentration (0-5000ppm) from PWM signal
- Configurable via Kconfig for default GPIO and capture mode

## Hardware Connection

Connect the CJ202 sensor to your ESP32 development board:

- CJ202 VCC → ESP32 3.3V or 5V (according to sensor specifications)
- CJ202 GND → ESP32 GND
- CJ202 PWM → ESP32 GPIO (default is GPIO 4, configurable via Kconfig or code)

## Usage

### 1. Add Component

In your project's `CMakeLists.txt` file, add the component path:

```cmake
set(EXTRA_COMPONENT_DIRS "path/to/components/cj202_co2_sensor")
```

### 2. Configure Component

Use `menuconfig` to configure the component:

```bash
idf.py menuconfig
```

Under `Component config → CJ202 CO2 Sensor Configuration`:
- Set the default GPIO pin
- Select the default capture mode

### 3. Code Example

```c
#include "cj202_co2_sensor.h"

void app_main(void)
{
    // Use default configuration
    cj202_config_t config = CJ202_DEFAULT_CONFIG();
    
    // Or customize configuration
    // config.gpio_num = 5;
    // config.mode = CJ202_MODE_GPIO_INTERRUPT;
    
    // Initialize sensor
    esp_err_t ret = cj202_init(&config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Sensor initialization failed");
        return;
    }
    
    // Read CO2 concentration
    uint32_t co2_ppm = cj202_get_ppm();
    printf("CO2 concentration: %u ppm\n", co2_ppm);
}
```

## API Reference

### Initialize Sensor

```c
esp_err_t cj202_init(const cj202_config_t *config);
```

### Get CO2 Concentration

```c
uint32_t cj202_get_ppm(void);
```

## Example Projects

A complete example is available in the `examples/cj202_example/` directory.

## Technical Details

The CJ202 sensor outputs CO2 concentration via PWM signal with the following characteristics:
- Period: 1004ms ±5%
- CO2 concentration calculation formula: Cppm = 5000 × (TH-2ms) / (TH+TL-4ms)
  - TH: High level time (ms)
  - TL: Low level time (ms)
  - Cppm: CO2 concentration (ppm)

## Compatibility

- GPIO Interrupt Mode is supported on all ESP32 series chips
- MCPWM Capture Mode is not available on ESP32C2 and ESP32C3, component automatically excludes this functionality on these platforms 