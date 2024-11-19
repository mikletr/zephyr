
#include "common.h"
#include "leds.h"
#include "send.h"

#include <zephyr/misc/lorem_ipsum.h>

LOG_MODULE_DECLARE(meshtalk, LOG_LEVEL_DBG);

const char lorem_ipsum[] = LOREM_IPSUM;
// LOREM_IPSUM_STRLEN 			1160

static uint32_t nCounter = 0;

static struct message msg;

static uint32_t nLenght = 260;//4 + 4 + 16;

//ZBUS_CHAN_DEFINE(your_channel, struct message, NULL, NULL, ZBUS_OBSERVERS_EMPTY, ZBUS_MSG_INIT(0));

void send_periodic_message(struct k_timer *timer)
{
    msg.lenght = nLenght; // 4

	msg.counter = nCounter++; // 4

    msg.data = lorem_ipsum; //"Periodic Message"; // 16          // Example message

    // Send the message to the appropriate channel

	sendData((const char *)(&msg), nLenght);
    //zbus_chan_pub(&your_channel, &msg, K_NO_WAIT);
	LOG_DBG("send: Sent %d bytes, packet number %d",  4 + 4 + 16, msg.counter);
}

K_TIMER_DEFINE(periodic_timer, send_periodic_message, NULL);

void start_periodic_message_timer(void)
{
    k_timer_start(&periodic_timer, K_SECONDS(1), K_SECONDS(1)); // Send every second
}
