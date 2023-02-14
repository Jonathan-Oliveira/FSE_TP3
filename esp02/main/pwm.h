#ifndef PWM
#define PWM
#include "driver/ledc.h"
#include "esp_err.h"

void pwm_config(gpio_num_t pin, uint32_t freq, uint32_t duty);
void set_duty(uint32_t duty);
#endif
