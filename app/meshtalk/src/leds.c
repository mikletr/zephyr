
#include "common.h"
#include "leds.h"


#define LED0_NODE DT_ALIAS(led0)
#define LED1_NODE DT_ALIAS(led1)
#define LED2_NODE DT_ALIAS(led2)
#define LED3_NODE DT_ALIAS(led3)


/*
 * A build error on this line means your board is unsupported.
 * See the sample documentation for information on how to fix this.
 */
static const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
static const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET(LED1_NODE, gpios);
static const struct gpio_dt_spec led2 = GPIO_DT_SPEC_GET(LED2_NODE, gpios);
static const struct gpio_dt_spec led3 = GPIO_DT_SPEC_GET(LED3_NODE, gpios);

void init_leds(){

    int ret = 0;

 	if (!gpio_is_ready_dt(&led0)) {
		return;
	}
	if (!gpio_is_ready_dt(&led1)) {
		return;
	}
	if (!gpio_is_ready_dt(&led2)) {
		return;
	}

	ret = gpio_pin_configure_dt(&led0, GPIO_OUTPUT_ACTIVE);
	ret = gpio_pin_configure_dt(&led1, GPIO_OUTPUT_ACTIVE);
	ret = gpio_pin_configure_dt(&led2, GPIO_OUTPUT_ACTIVE);
	ret = gpio_pin_configure_dt(&led3, GPIO_OUTPUT_ACTIVE);

	if (ret < 0) {
		return;
	}

	set_leds_off();
}

void set_leds_off(){
	
	gpio_pin_set_dt(&led0, false);
	gpio_pin_set_dt(&led1, false);
	gpio_pin_set_dt(&led2, false);
	gpio_pin_set_dt(&led3, false);
}

void set_blue(){
	
	gpio_pin_set_dt(&led2, true);
}
void set_red(){

    gpio_pin_set_dt(&led0, true);
}
void set_green(){

	gpio_pin_set_dt(&led1, true);
}

void set_small_green(){

	gpio_pin_set_dt(&led3, true);
}

void toggle_small_green(){
    gpio_pin_toggle_dt(&led3);
}