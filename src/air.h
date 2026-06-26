/*
 * Air Sensor
 * Original code by WHowe <github.com/whowechina>
 * Modified by Infecta <github.com/Infecta>
 */

#ifndef AIR_H
#define AIR_H

#include <stdint.h>
#include <stdbool.h>

void air_init();
uint16_t air_ir_raw(uint8_t index);
bool air_ir_blocked(uint8_t index);
uint8_t air_bitmap();
void air_update();

#endif
