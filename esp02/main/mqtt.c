#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_system.h"
#include "esp_event.h"
#include "esp_netif.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_log.h"
#include "mqtt_client.h"
#include "cJSON.h"

#include "mqtt.h"
#include "nvs.h"
#include "gpio_setup.h"

#define TAG "MQTT"

#define MQTT_SERVER_URI CONFIG_MQTT_SERVER_URI
#define MQTT_USERNAME CONFIG_MQTT_USERNAME
#define LED2_GPIO 22

extern SemaphoreHandle_t conexaoMQTTSemaphore;
esp_mqtt_client_handle_t client;

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0)
    {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

static void set_botao(char *resp_topic, int parameter)
{
    char resp_msg[50];
    uint32_t button_state;
    ESP_LOGI(TAG, "Setando o butao para %d", parameter);
    grava_valor_nvs("button_state", parameter);
    button_state = le_valor_nvs("button_state");
    digitalWrite(LED2_GPIO, button_state);

    snprintf(resp_msg, 49, "{\"status\": %ld}", button_state);
    mqtt_envia_mensagem(resp_topic, resp_msg);
}
static void get_botao(char *resp_topic, int parameter)
{
    char resp_msg[50];
    uint32_t button_state;
    button_state = le_valor_nvs("button_state");
    snprintf(resp_msg, 49, "{\"status\": %ld}", button_state);
    mqtt_envia_mensagem(resp_topic, resp_msg);
}

static void set_reed(char *resp_topic, int parameter)
{
    char resp_msg[50];
    uint32_t reed_state;
    ESP_LOGI(TAG, "Alterando o reed para %d", parameter);
    grava_valor_nvs("reed_state", parameter);
    reed_state = le_valor_nvs("reed_state");

    snprintf(resp_msg, 49, "{\"status\": %ld}", reed_state);
    mqtt_envia_mensagem(resp_topic, resp_msg);
}

static void execute_rpc_request(esp_mqtt_event_handle_t event,
                                char *method,
                                int method_len,
                                int parameter,
                                int topic_id)
{
    char resp_topic[50];

    snprintf(resp_topic, 49, "v1/devices/me/rpc/response/%d", topic_id);

    ESP_LOGI(TAG, "Parameter: %d\n", parameter);
    if (strncmp(method, "setBotao", 50) == 0)
    {
        set_botao(resp_topic, parameter);
    }
    if (strncmp(method, "getBotao", 50) == 0)
    {
        get_botao(resp_topic, parameter);
    }
    else if (strncmp(method, "setReed", 50) == 0)
    {
        set_reed(resp_topic, parameter);
    }
    else if (strncmp(method, "ping", 50) == 0)
    {
        mqtt_envia_mensagem(resp_topic, "{\"code\": 200, \"message\": \"pong\"}");
    }
    else if (strncmp(method, "getTeste", 50) == 0)
    {
        char resp_msg[50];
        // generate a random number between 0 and 100
        int random_number = rand() % 100;
        snprintf(resp_msg, 49, "{\"params\": %d}", random_number);
        mqtt_envia_mensagem(resp_topic, resp_msg);
    }
    else if (strncmp(method, "setLEDRed", 50) == 0)
    {
        char resp_msg[50];
        // generate a random number between 0 and 100
        int random_number = rand() % 100;
        snprintf(resp_msg, 49, "{ \"params\": %d}", random_number);
        mqtt_envia_mensagem(resp_topic, resp_msg);
    }
    else
    {
        ESP_LOGW(TAG, "method: '%.*s' not implemented", method_len, method);
        mqtt_envia_mensagem(resp_topic, "{\"status\": \"method not implemented\"}");
    }
}

static void handle_rpc_request_data(esp_mqtt_event_handle_t event, int topic_id)
{
    int parameter;
    char *method = NULL;

    cJSON *json = NULL;
    cJSON *json_method = NULL;
    cJSON *json_param = NULL;
    json = cJSON_ParseWithLength(event->data, event->data_len);
    if (json == NULL)
    {
        ESP_LOGE(TAG, "Invalid RPC request data '%.*s'", event->data_len, event->data);
    }
    else
    {
        json_method = cJSON_GetObjectItemCaseSensitive(json, "method");
        if (cJSON_IsString(json_method) && (json_method->valuestring != NULL))
        {
            method = json_method->valuestring;
            json_param = cJSON_GetObjectItemCaseSensitive(json, "params");

            if (cJSON_IsNumber(json_param))
            {
                parameter = json_param->valueint;
                execute_rpc_request(event, method, strnlen(method, 50), parameter, topic_id);
            }
            else if (cJSON_IsBool(json_param))
            {
                parameter = json_param->valueint;
                execute_rpc_request(event, method, strnlen(method, 50), parameter, topic_id);
            }
            else if (cJSON_IsString(json_param) && (json_param->valuestring != NULL))
            {
                parameter = atoi(json_param->valuestring);
                execute_rpc_request(event, method, strnlen(method, 50), parameter, topic_id);
            }
            else if (cJSON_IsNull(json_param))
            {
                parameter = 0;
                execute_rpc_request(event, method, strnlen(method, 50), parameter, topic_id);
            }
            else
            {
                ESP_LOGE(TAG, "Invalid while parsing param");
            }
        }
        else
        {
            ESP_LOGE(TAG, "Error while parsing method");
        }
    }

    cJSON_Delete(json);
}
static void handle_event_topic(esp_mqtt_event_handle_t event)
{
    int rst;
    int topic_id;

    rst = sscanf(event->topic, "v1/devices/me/rpc/request/%d", &topic_id);
    if (rst == 1)
    {
        handle_rpc_request_data(event, topic_id);
    }
    else
    {
        ESP_LOGE(TAG, "Event '%.*s' is not implemented", event->topic_len, event->topic);
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, (int)event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        xSemaphoreGive(conexaoMQTTSemaphore);

        msg_id = esp_mqtt_client_subscribe(client, "v1/devices/me/rpc/request/+", 0);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        msg_id = esp_mqtt_client_subscribe(client, "v1/devices/me/attributes/response/+", 0);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        // set_mqtt_connected(true);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        // set_mqtt_connected(false);
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        ESP_LOGI(TAG, "TOPIC=%.*s\r\n", event->topic_len, event->topic);
        ESP_LOGI(TAG, "DATA=%.*s\r\n", event->data_len, event->data);

        handle_event_topic(event);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT)
        {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno", event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

void mqtt_start()
{
    esp_mqtt_client_config_t mqtt_config = {
        .broker.address.uri = MQTT_SERVER_URI,
        .credentials.username = MQTT_USERNAME};
    client = esp_mqtt_client_init(&mqtt_config);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}

void mqtt_envia_mensagem(char *topico, char *mensagem)
{
    int message_id = esp_mqtt_client_publish(client, topico, mensagem, 0, 1, 0);
    ESP_LOGI(TAG, "Mensagem enviada, ID: %d", message_id);
    ESP_LOGI(TAG, "Mensagem enviada, Topico: %s", topico);
    ESP_LOGI(TAG, "Mensagem enviada, Mensagem: %s", mensagem);
}
