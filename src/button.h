/*
 * Controller Buttons
 * WHowe <github.com/whowechina>
 */

#ifndef BUTTON_H
#define BUTTON_H

#include <stdint.h>
#include <stdbool.h>

void button_init();
uint8_t button_num();
void button_update();
uint16_t button_read();
void button_inject(uint16_t mask);

#endif
