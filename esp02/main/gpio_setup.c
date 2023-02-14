#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "driver/ledc.h"

#include "gpio_setup.h"
#include "adc_module.h"

const static char *TAG = "GPIO_SETUP";

void pinMode(gpio_num_t pin, int mode)
{
  gpio_config_t io_conf = {};
  switch (mode)
  {
  case GPIO_OUTPUT:
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << pin);
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);
    break;
  case GPIO_INPUT:
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << pin);
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);
    break;
  case GPIO_INPUT_PULLUP:
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << pin);
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);
    break;
  case GPIO_INPUT_PULLDOWN:
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << pin);
    io_conf.intr_type = GPIO_INTR_POSEDGE;
    io_conf.pull_down_en = 1;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);
    break;
  case GPIO_ANALOG:
    adc_config_pin(pin);
    break;
  default:
    ESP_LOGW(TAG, "Unable to configure pin %d", pin);
    break;
  }
}

void digitalWrite(gpio_num_t pin, uint32_t level)
{
  gpio_set_level(pin, level);
}

uint32_t digitalRead(gpio_num_t pin)
{
  return gpio_get_level(pin);
}
