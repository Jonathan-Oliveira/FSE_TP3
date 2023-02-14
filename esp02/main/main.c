#include <stdio.h>
#include <string.h>
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_timer.h"
#include <time.h>
#include <sys/time.h>
#include "esp_http_client.h"
#include "esp_log.h"
#include "freertos/semphr.h"
#include "esp_sleep.h"
#include "driver/uart.h"
#include "driver/ledc.h"

#include "esp32/rom/uart.h"

#include "driver/rtc_io.h"

#include "wifi.h"
#include "mqtt.h"
#include "gpio_setup.h"
#include "adc_module.h"
#include "nvs.h"
#include "pwm.h"

// Define the GPIO pins for the button and reed switch
#define BUTTON_GPIO 5
#define REED_SWITCH_GPIO 23
#define REED_SWITCH_GPIO_ADC ADC_CHANNEL_6
#define LED_GPIO 2
#define LED2_GPIO 22
#define ESP_INTR_FLAG_DEFAULT 0

bool mqtt_connected = false;

SemaphoreHandle_t conexaoWifiSemaphore;
SemaphoreHandle_t conexaoMQTTSemaphore;

QueueHandle_t filaDeInterrupcao;

static void IRAM_ATTR gpio_isr_handler(void *arg)
{
  int pino = (int)arg;
  xQueueSendFromISR(filaDeInterrupcao, &pino, NULL);
}

void set_states(void)
{
  int button_state = le_valor_nvs("button_state");
  digitalWrite(LED2_GPIO, button_state);
}
void conectadoWifi(void *params)
{
  while (true)
    if (xSemaphoreTake(conexaoWifiSemaphore, portMAX_DELAY))
      mqtt_start();
}

void trataInterrupcaoButao(void *args)
{

  int pino;
  int button_state;
  int reed_switch_state;

  char mensagem[100];
  while (true)
  {
    if (xQueueReceive(filaDeInterrupcao, &pino, portMAX_DELAY))
    {
      int estado = digitalRead(pino);
      if (estado == 1)
      {
        gpio_isr_handler_remove(pino);
        while (digitalRead(pino) == estado)
        {
          vTaskDelay(50 / portTICK_PERIOD_MS);
        }
        if (pino == BUTTON_GPIO)
        {
          button_state = le_valor_nvs("button_state");
          // if dont have value in nvs, set to 0
          ESP_LOGI("INTERRUPCAO", "Botao pressionado");
          button_state = button_state == -1 ? 0 : button_state;
          button_state = !button_state;

          grava_valor_nvs("button_state", button_state);
          digitalWrite(LED2_GPIO, button_state);
          if (mqtt_connected)
          {
            sprintf(mensagem, "{\"botao\": %d}", button_state);
            ESP_LOGI("MQTT", "Enviando mensagem: %s", mensagem);
            mqtt_envia_mensagem("v1/devices/me/attributes", mensagem);
            ESP_LOGD("INTERRUPCAO", "Botao pressionado");
          }
        }
        else if (pino == REED_SWITCH_GPIO)
        {
          reed_switch_state = le_valor_nvs("reed_state");
          // if dont have value in nvs, set to 0
          reed_switch_state == -1 ? reed_switch_state = 0 : reed_switch_state;
          reed_switch_state = !reed_switch_state;
          grava_valor_nvs("reed_state", reed_switch_state);
          if (mqtt_connected)
          {
            sprintf(mensagem, "{\"sensor\": %d}", reed_switch_state);
            ESP_LOGI("MQTT", "Enviando mensagem: %s sensor magnético", mensagem);
            mqtt_envia_mensagem("v1/devices/me/attributes", mensagem);
            ESP_LOGD("INTERRUPCAO", "Sensor magnético ativado");
          }
        }
        vTaskDelay(50 / portTICK_PERIOD_MS);
        gpio_isr_handler_add(pino, gpio_isr_handler, (void *)pino);
      }
    }
  }
}

void trataComunicacaoComServidor(void *params)
{

  char mensagem[50];
  if (xSemaphoreTake(conexaoMQTTSemaphore, portMAX_DELAY))
  {
    mqtt_connected = true;
    while (true)
    {

      int reed_signal = analogRead(REED_SWITCH_GPIO_ADC);

      // pwm variable 0 to 100
      uint32_t duty = 100 - (reed_signal * 100 / 4095);
      set_duty(duty);
      ESP_LOGI("PWM", "Valor do PWM: %ld", duty);

      sprintf(mensagem, "{\"sensor\": %d}", reed_signal);
      mqtt_envia_mensagem("v1/devices/me/telemetry", mensagem);

      vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
  }
}

#ifndef CONFIG_BATTERY_MODE
void app_main(void)
{
  // Inicializa o NVS
  init_nvs();
  adc_init(ADC_UNIT_1);

  // Configura os pinos
  pinMode(LED_GPIO, GPIO_OUTPUT);
  pinMode(LED2_GPIO, GPIO_OUTPUT);

  pinMode(BUTTON_GPIO, GPIO_INPUT_PULLDOWN);
  pinMode(REED_SWITCH_GPIO, GPIO_INPUT_PULLDOWN);
  pinMode(REED_SWITCH_GPIO_ADC, GPIO_ANALOG);

  // Inicializa o WiFi
  conexaoWifiSemaphore = xSemaphoreCreateBinary();
  conexaoMQTTSemaphore = xSemaphoreCreateBinary();
  wifi_start();

  // Inicializa o MQTT
  xTaskCreate(&conectadoWifi, "Conexão ao MQTT", 4096, NULL, 1, NULL);
  xTaskCreate(&trataComunicacaoComServidor, "Comunicação com Broker", 4096, NULL, 1, NULL);

  // Inicializa as tarefas
  xTaskCreate(&trataInterrupcaoButao, "Trata Interrupção Botão", 2048, NULL, 1, NULL);

  filaDeInterrupcao = xQueueCreate(10, sizeof(int));
  xTaskCreate(&trataInterrupcaoButao, "Trata Interrupção Botão", 2048, NULL, 1, NULL);

  // Inicializa o serviço de interrupção
  gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
  gpio_isr_handler_add(BUTTON_GPIO, gpio_isr_handler, (void *)BUTTON_GPIO);
  gpio_isr_handler_add(REED_SWITCH_GPIO, gpio_isr_handler, (void *)REED_SWITCH_GPIO);

  pwm_config(LED_GPIO, 1000, 0);

  set_states();
}
#else
void app_main()
{
  ESP_LOGW("BATTERY", "Battery mode");

  TaskHandle_t conectadoWifiHandle;
  TaskHandle_t trataComunicacaoComServidorHandle;
  TaskHandle_t button_taskHandle;
  conexaoWifiSemaphore = xSemaphoreCreateBinary();
  conexaoMQTTSemaphore = xSemaphoreCreateBinary();
  // Inicializa o NVS
  init_nvs();
  adc_init(ADC_UNIT_1);

  wifi_start();

  xTaskCreate(conectadoWifi, "Conexão ao MQTT", 4096, NULL, 1, &conectadoWifiHandle);
  xTaskCreate(trataComunicacaoComServidor, "Comunicação com Broker", 4096, NULL, 1, &trataComunicacaoComServidorHandle);

  filaDeInterrupcao = xQueueCreate(10, sizeof(int));
  xTaskCreate(trataInterrupcaoButao, "Trata Interrupção Botão", 2048, NULL, 1, &button_taskHandle);

  pinMode(LED_GPIO, GPIO_OUTPUT);

  pinMode(BUTTON_GPIO, GPIO_INPUT_PULLDOWN);
  pinMode(REED_SWITCH_GPIO, GPIO_INPUT_PULLDOWN);
  pinMode(REED_SWITCH_GPIO_ADC, GPIO_ANALOG);

  gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
  gpio_isr_handler_add(BUTTON_GPIO, gpio_isr_handler, (void *)BUTTON_GPIO);
  gpio_isr_handler_add(REED_SWITCH_GPIO, gpio_isr_handler, (void *)REED_SWITCH_GPIO);

  pwm_config(LED_GPIO, 1000, 0);

  gpio_wakeup_enable(BUTTON_GPIO, GPIO_INTR_LOW_LEVEL);
  gpio_wakeup_enable(REED_SWITCH_GPIO, GPIO_INTR_LOW_LEVEL);
  esp_sleep_enable_gpio_wakeup();

  while (true)
  {
    vTaskDelay(5000 / portTICK_PERIOD_MS);
    ESP_LOGW("BATTERY", "Going to sleep now");
    ESP_LOGW("BATTERY", "Suspending tasks");
    gpio_isr_handler_remove(BUTTON_GPIO);
    gpio_isr_handler_remove(REED_SWITCH_GPIO);
    vTaskSuspend(conectadoWifiHandle);
    vTaskSuspend(trataComunicacaoComServidorHandle);
    vTaskSuspend(button_taskHandle);
    uart_wait_tx_idle_polling(CONFIG_ESP_CONSOLE_UART_NUM);
    int64_t time = esp_timer_get_time();
    esp_light_sleep_start();
    time = esp_timer_get_time() - time;

    ESP_LOGW("BATTERY", "Waking up");
    ESP_LOGI("BATTERY", "Woke up after %lld ms", time / 1000);

    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    ESP_LOGI("BATTERY", "Wakeup reason: %s", wakeup_reason == ESP_SLEEP_WAKEUP_GPIO ? "GPIO" : "OTHER");

    ESP_LOGI("BATTERY", "Resuming tasks");

    gpio_isr_handler_add(BUTTON_GPIO, gpio_isr_handler, (void *)BUTTON_GPIO);
    gpio_isr_handler_add(REED_SWITCH_GPIO, gpio_isr_handler, (void *)REED_SWITCH_GPIO);
    vTaskResume(conectadoWifiHandle);
    vTaskResume(trataComunicacaoComServidorHandle);
    vTaskResume(button_taskHandle);

    ESP_LOGW("BATTERY", "Woke up");
    vTaskDelay(5000 / portTICK_PERIOD_MS);
  }
}
#endif