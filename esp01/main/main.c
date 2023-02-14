#include <stdio.h>
#include <string.h>
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "freertos/semphr.h"

#include "wifi.h"
#include "mqtt.h"
#include "gpio_setup.h"
#include "adc_module.h"
#include "rotary_encoder.h"
#include "nvs.h"

#define ENCODER_BUTTON_PIN 22

SemaphoreHandle_t conexaoWifiSemaphore;
SemaphoreHandle_t conexaoMQTTSemaphore;

void conectadoWifi(void * params) {
	while(true) {
		if(xSemaphoreTake(conexaoWifiSemaphore, portMAX_DELAY)) {
			// Processamento Internet
			mqtt_start();
		}
	}
}

void trataComunicacaoComServidor(void * params) {
	char mensagem[50];

	adc_init(ADC_UNIT_1);
	pinMode(LDR_CHANNEL, GPIO_ANALOG);
	pinMode(GPIO_ENCODER_SW, GPIO_INPUT_PULLUP);

	ESP_ERROR_CHECK(gpio_install_isr_service(0));
	rotary_encoder_info_t info = { 0 };
    ESP_ERROR_CHECK(rotary_encoder_init(&info, GPIO_ENCODER_DT, GPIO_ENCODER_CLK));

	char cores[3] = {'r', 'g', 'b'};
	int cor_atual = 0;
	char sw_atual = 0;

	QueueHandle_t event_queue = rotary_encoder_create_queue();
    ESP_ERROR_CHECK(rotary_encoder_set_queue(&info, event_queue));

	gpio_reset_pin(ENCODER_BUTTON_PIN);
    gpio_set_direction(ENCODER_BUTTON_PIN, GPIO_MODE_OUTPUT);

	if(xSemaphoreTake(conexaoMQTTSemaphore, portMAX_DELAY)) {
		while(true) {
			int sw = digitalRead(GPIO_ENCODER_SW);

			if (sw == 0) {
				sw_atual = !sw_atual;
				digitalWrite(ENCODER_BUTTON_PIN, sw_atual);

				cor_atual++;

				if (cor_atual > 2) {
					cor_atual = 0;
				}
			}

			if (cor_atual == 0) {
				info.state.position = le_valor_nvs("r");

			} else if (cor_atual == 1) {
				info.state.position = le_valor_nvs("g");

			} else if (cor_atual == 2) {
				info.state.position = le_valor_nvs("b");
			}

			rotary_encoder_event_t event = { 0 };
			if (xQueueReceive(event_queue, &event, 1000 / portTICK_PERIOD_MS) == pdTRUE) {
				if (event.state.position > 100) {
					info.state.position = 100;
					event.state.position = 100;
				} else if (event.state.position < 0) {
					info.state.position = 0;
					event.state.position = 0;
				}
				
				if (cor_atual == 0) {
					grava_valor_nvs("r", event.state.position);

				} else if (cor_atual == 1) {
					grava_valor_nvs("g", event.state.position);

				} else if (cor_atual == 2) {
					grava_valor_nvs("b", event.state.position);
				}

				sprintf(mensagem, "{\"%c\": %ld}", cores[cor_atual], event.state.position);
				mqtt_envia_mensagem("v1/devices/me/attributes", mensagem);
        	}

			int ldr = analogRead(LDR_CHANNEL);
			sprintf(mensagem, "{\"ldr\": %d}",ldr);
			mqtt_envia_mensagem("v1/devices/me/telemetry", mensagem);
			grava_valor_nvs("ldr", ldr);

			vTaskDelay(300 / portTICK_PERIOD_MS);
		}
	}
}

void app_main(void) {

	init_nvs();

	conexaoWifiSemaphore = xSemaphoreCreateBinary();
	conexaoMQTTSemaphore = xSemaphoreCreateBinary();

	wifi_start();

	xTaskCreate(&conectadoWifi, "Conexão ao MQTT", 4096, NULL, 1, NULL);
	xTaskCreate(&trataComunicacaoComServidor, "Comunicação com Broker", 4096, NULL, 1, NULL);
}
