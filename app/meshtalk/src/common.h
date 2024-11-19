
#ifndef _COMMON_H_
#define _COMMON_H_


#include <zephyr/kernel.h>

#include <zephyr/drivers/gpio.h>

#include <zephyr/logging/log.h>

#define MY_PORT 4242

#define PEER_PORT 4242

#define STACK_SIZE 2048

#define UDP_STACK_SIZE 2048

#define PRINT_PROGRESS true


#if defined(CONFIG_NET_TC_THREAD_COOPERATIVE)
#define THREAD_PRIORITY K_PRIO_COOP(CONFIG_NUM_COOP_PRIORITIES - 1)
#else
#define THREAD_PRIORITY K_PRIO_PREEMPT(8)
#endif

#define RECV_BUFFER_SIZE 1280
#define STATS_TIMER 60 /* How often to print statistics (in seconds) */

#define APP_BMEM
#define APP_DMEM

struct message {
	uint32_t lenght;
	uint32_t counter;

    const char *data;
};

struct rx_data {
	const char *proto;

	struct {
		int sock;
		char recv_buffer[RECV_BUFFER_SIZE];
		uint32_t counter;
		atomic_t bytes_received;
		struct k_work_delayable stats_print;
	} udp;
};

struct rx_configs {
	struct rx_data ipv6;
};

extern struct rx_configs rx_conf;

// ------------------- client -------------
struct udp_control {
	struct k_poll_signal tx_signal;
	struct k_timer tx_timer;
	struct k_timer rx_timer;
};

struct tx_data {
	const char *proto;

	struct {
		int sock;
		uint32_t expecting;
		uint32_t counter;
		uint32_t mtu;
		struct udp_control *ctrl;
	} udp;
};

struct tx_configs {
	struct tx_data ipv6;
};

extern struct tx_configs tx_conf;

void init_udp(void);
int start_udp_tx(void);
int process_udp(void);
void stop_udp_tx(void);


// --------------------------------
void start_udp_rx(void);
void stop_udp_rx(void);

void quit(void);

int sendData(const char *data, int len);

#endif //_COMMON_H_