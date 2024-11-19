#ifndef _LEDS_H_
#define _LEDS_H_

#include <zephyr/kernel.h>

#include <zephyr/drivers/gpio.h>



void init_leds(void);
void set_blue(void);
void set_red(void);
void set_green(void);
void set_leds_off(void);
void set_small_green(void);
void toggle_small_green(void);


#endif //_LEDS_H_