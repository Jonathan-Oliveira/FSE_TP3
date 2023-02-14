#ifndef _GPIO_SETUP
#define _GPIO_SETUP

#include <stdio.h>
#include "driver/gpio.h"

#define GPIO_OUTPUT          0
#define GPIO_INPUT           1
#define GPIO_INPUT_PULLUP    2
#define GPIO_INPUT_PULLDOWN  3
#define GPIO_ANALOG          4

#define GPIO_ENCODER_CLK 5
#define GPIO_ENCODER_DT 18
#define GPIO_ENCODER_SW 19


void pinMode(gpio_num_t pin, int mode);
void digitalWrite(gpio_num_t pin, uint32_t level);
uint32_t digitalRead(gpio_num_t pin);

#endif