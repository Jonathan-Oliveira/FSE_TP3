#include <stdio.h>
#include "driver/ledc.h"
#include "esp_err.h"
#include "pwm.h"
#include "gpio_setup.h"

void pwm_config(gpio_num_t pin, uint32_t freq, uint32_t duty)
{
  pinMode(pin, GPIO_OUTPUT);
  ledc_timer_config_t ledc_timer = {
      .duty_resolution = LEDC_TIMER_13_BIT, // resolution of PWM duty
      .freq_hz = freq,                      // frequency of PWM signal
      .speed_mode = LEDC_HIGH_SPEED_MODE,   // timer mode
      .timer_num = LEDC_TIMER_0,            // timer index
      .clk_cfg = LEDC_AUTO_CLK,             // Auto select the source clock
  };
  // Set configuration of timer0 for high speed channels
  ledc_timer_config(&ledc_timer);

  ledc_channel_config_t ledc_channel = {
      .channel = LEDC_CHANNEL_0,
      .duty = duty,
      .gpio_num = pin,
      .speed_mode = LEDC_HIGH_SPEED_MODE,
      .hpoint = 0,
      .timer_sel = LEDC_TIMER_0,
  };
  // Set LED Controller with previously prepared configuration
  ledc_channel_config(&ledc_channel);
}

void set_duty(uint32_t duty)
{
  ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, duty);
  ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0);
}