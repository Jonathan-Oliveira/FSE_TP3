#ifndef _NVS_H
#define _NVS_H

#include "esp_system.h"

void init_nvs();
int32_t le_valor_nvs(char *prop);
void grava_valor_nvs(char *chave, int32_t valor);

#endif