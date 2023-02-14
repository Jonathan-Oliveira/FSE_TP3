#ifndef _ADC_MODULE_
#define _ADC_MODULE_

#include "esp_adc/adc_oneshot.h"

#define LDR_CHANNEL ADC_CHANNEL_0

void adc_init();
void adc_config_pin(adc_channel_t channel);
void adc_deinit();
int analogRead(adc_channel_t channel);

#endif