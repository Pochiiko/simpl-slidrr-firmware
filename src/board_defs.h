/*
 * Board Definitions
 * Original code by WHowe <github.com/whowechina>
 * Modified by Infecta <github.com/Infecta>
 */

#if defined BOARD_SIMPL_SLIDRR

// i2c defs
#define I2C_PORT i2c0
#define I2C_SDA 16
#define I2C_SCL 17
#define I2C_FREQ 620*1000

// external i2c hub pin
#define I2C_HUB_EN 19

// ir tower pin defs
#define IR_GROUP_ABC_GPIO { 3, 4, 5 }
#define IR_SIG_ADC_CHANNEL { 0, 1 }

// aux button defs
#define BUTTON_DEF { 13, 14, 15 }
#else

#endif
