idf_component_register(
    SRCS 
        "src/cj202_co2_sensor.c"
        "src/cj202_gpio.c"
        "src/cj202_common.c"
        "src/cj202_mcpwm.c"
    INCLUDE_DIRS 
        "include"
    REQUIRES 
        driver esp_timer
) 