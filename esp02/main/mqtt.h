#ifndef MQTT_H
#define MQTT_H

void mqtt_start();

void mqtt_envia_mensagem(char *topico, char *mensagem);

bool get_mqtt_connected(void);

#endif