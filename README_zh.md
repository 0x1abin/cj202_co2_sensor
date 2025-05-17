# CJ202 CO2传感器组件

该组件提供了与CJ202 CO2传感器通信的功能，支持PWM信号捕获和处理。

## 特性

- 支持两种捕获模式：
  - GPIO中断模式：适用于所有ESP32系列芯片
  - MCPWM捕获模式：适用于支持MCPWM捕获功能的ESP32系列芯片（不包括ESP32C2和ESP32C3）
- 根据PWM信号计算CO2浓度 (0-5000ppm)
- 通过Kconfig可配置默认GPIO和捕获模式

## 硬件连接

将CJ202传感器连接到ESP32开发板：

- CJ202 VCC → ESP32 3.3V 或 5V (根据传感器规格)
- CJ202 GND → ESP32 GND
- CJ202 PWM → ESP32 GPIO (默认GPIO 4，可通过Kconfig或代码配置)

## 使用方法

### 1. 添加组件

在你的项目的`CMakeLists.txt`文件中添加组件路径：

```cmake
set(EXTRA_COMPONENT_DIRS "path/to/components/cj202_co2_sensor")
```

### 2. 配置组件

使用`menuconfig`配置组件：

```bash
idf.py menuconfig
```

在`Component config → CJ202 CO2 Sensor Configuration`中：
- 设置默认GPIO引脚
- 选择默认捕获模式

### 3. 代码示例

```c
#include "cj202_co2_sensor.h"

void app_main(void)
{
    // 使用默认配置
    cj202_config_t config = CJ202_DEFAULT_CONFIG();
    
    // 或自定义配置
    // config.gpio_num = 5;
    // config.mode = CJ202_MODE_GPIO_INTERRUPT;
    
    // 初始化传感器
    esp_err_t ret = cj202_init(&config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "传感器初始化失败");
        return;
    }
    
    // 读取CO2浓度
    uint32_t co2_ppm = cj202_get_ppm();
    printf("CO2浓度: %u ppm\n", co2_ppm);
}
```

## API参考

### 初始化传感器

```c
esp_err_t cj202_init(const cj202_config_t *config);
```

### 获取CO2浓度

```c
uint32_t cj202_get_ppm(void);
```

## 示例项目

完整示例位于`examples/cj202_example/`目录。

## 技术细节

CJ202传感器使用PWM信号输出CO2浓度，信号特性：
- 周期：1004ms ±5%
- CO2浓度计算公式：Cppm = 5000 × (TH-2ms) / (TH+TL-4ms)
  - TH: 高电平时间(ms)
  - TL: 低电平时间(ms)
  - Cppm: CO2浓度(ppm)

## 兼容性

- 所有ESP32系列芯片均支持GPIO中断模式
- ESP32C2和ESP32C3不支持MCPWM捕获模式，组件会自动使用条件编译排除该功能

[English Documentation](./README.md) 