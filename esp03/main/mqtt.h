/*
 *  ESP32 MQTT Thingsboard
 *  Copyright (C) 2022 Wellington Jonatan <wellpriz at gmail.com>
 *  This file is part of ESP32 MQTT Thingsboard.

 *  ESP32 MQTT Thingsboard is free software: you can redistribute it and/or
 *  modify it under the terms of the GNU Affero General Public License as
 *  published by the Free Software Foundation, either version 3 of the License,
 *  or (at your option) any later version.

 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Affero General Public License for more details.

 *  You should have received a copy of the GNU Affero General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef MQTT_H
#define MQTT_H

void mqtt_start();

void mqtt_envia_mensagem(char *topico, char *mensagem);

#endif
