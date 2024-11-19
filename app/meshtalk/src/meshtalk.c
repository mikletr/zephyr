/* echo-server.c - Networking echo server */

/*
 * Copyright (c) 2016 Intel Corporation.
 * Copyright (c) 2018 Nordic Semiconductor ASA.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

//#include <zephyr/logging/log.h>

#include "common.h"

//#include <zephyr/kernel.h>
#include <zephyr/linker/sections.h>
#include <errno.h>
#include <zephyr/shell/shell.h>

#include <zephyr/net/net_core.h>
#include <zephyr/net/tls_credentials.h>

#include <zephyr/net/socket.h>
#include <zephyr/posix/sys/eventfd.h>
#include <zephyr/net/net_if.h>

#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/conn_mgr_monitor.h>

#include "leds.h"
#include "send.h"

#include <zephyr/zbus/zbus.h>



#include <zephyr/drivers/gpio.h>


LOG_MODULE_REGISTER(meshtalk, LOG_LEVEL_DBG);


#define APP_BANNER "Run meshtalk"



/* 1000 msec = 1 sec */
#define SLEEP_TIME_MS   1000

#define INVALID_SOCK (-1)


static struct k_sem quit_lock;
static struct net_mgmt_event_callback mgmt_cb;
static bool connected;
K_SEM_DEFINE(run_app, 0, 1);
static bool want_to_quit;

#define EVENT_MASK (NET_EVENT_L4_CONNECTED | \
		    NET_EVENT_L4_DISCONNECTED)

#define IPV6_EVENT_MASK (NET_EVENT_IPV6_ADDR_ADD | \
			 NET_EVENT_IPV6_ADDR_DEPRECATED)

APP_DMEM struct tx_configs tx_conf = {
	.ipv6 = {
		.proto = "IPv6",
		.udp.sock = INVALID_SOCK,
	},
};

APP_DMEM struct rx_configs rx_conf = {
	.ipv6 = {
		.proto = "IPv6",
	},
};


static APP_BMEM struct pollfd fds[1 + 4];
static APP_BMEM int nfds;

static APP_BMEM bool connected;
static APP_BMEM bool need_restart;

//K_SEM_DEFINE(run_app, 0, 1);

static struct net_mgmt_event_callback mgmt_cb;
static struct net_mgmt_event_callback ipv6_mgmt_cb;

static void prepare_fds(void)
{
	nfds = 0;

	/* eventfd is used to trigger restart */
	fds[nfds].fd = eventfd(0, 0);
	fds[nfds].events = POLLIN;
	nfds++;

	if (tx_conf.ipv6.udp.sock >= 0) {
		fds[nfds].fd = tx_conf.ipv6.udp.sock;
		fds[nfds].events = POLLIN;
	}
}

static void wait(void)
{
	int ret;

	/* Wait for event on any socket used. Once event occurs,
	 * we'll check them all.
	 */
	ret = poll(fds, nfds, -1);

	if (ret < 0) {
		static bool once;

		if (!once) {
			once = true;
			LOG_ERR("Error in poll:%d", errno);
		}

		return;
	}
}

static int start_udp_and_tcp(void)
{
	int ret;

	LOG_INF("Starting...");

	if (IS_ENABLED(CONFIG_NET_UDP)) {
		
		start_udp_rx();

		ret = start_udp_tx();

		if (ret < 0) {
			return ret;
		}
	}

	prepare_fds();

	return 0;
}

static void stop_udp_and_tcp(void)
{
	LOG_INF("Stopping...");

	if (IS_ENABLED(CONFIG_NET_UDP)) {
		stop_udp_tx();
		stop_udp_rx();
	}
}

static int run_udp_and_tcp(void)
{
	int ret;

	wait();

	if (IS_ENABLED(CONFIG_NET_UDP)) {
		ret = process_udp();
		if (ret < 0) {
			return ret;
		}
	}

	return 0;
}

static int check_our_ipv6_sockets(int sock,
				  struct in6_addr *deprecated_addr)
{
	struct sockaddr_in6 addr = { 0 };
	socklen_t addrlen = sizeof(addr);
	int ret;

	if (sock < 0) {
		return -EINVAL;
	}

	ret = getsockname(sock, (struct sockaddr *)&addr, &addrlen);

	if (ret != 0) {
		return -errno;
	}

	if (!net_ipv6_addr_cmp(deprecated_addr, &addr.sin6_addr)) {
		return -ENOENT;
	}

	need_restart = true;

	return 0;
}

static void ipv6_event_handler(struct net_mgmt_event_callback *cb,
			       uint32_t mgmt_event, struct net_if *iface)
{
	static char addr_str[INET6_ADDRSTRLEN];

	if (!IS_ENABLED(CONFIG_NET_IPV6_PE)) {
		return;
	}

	if ((mgmt_event & IPV6_EVENT_MASK) != mgmt_event) {
		return;
	}

	if (cb->info == NULL ||
	    cb->info_length != sizeof(struct in6_addr)) {
		return;
	}

	if (mgmt_event == NET_EVENT_IPV6_ADDR_ADD) {
		struct net_if_addr *ifaddr;
		struct in6_addr added_addr;

		memcpy(&added_addr, cb->info, sizeof(struct in6_addr));

		ifaddr = net_if_ipv6_addr_lookup(&added_addr, &iface);
		if (ifaddr == NULL) {
			return;
		}

		/* Wait until we get a temporary address before continuing after
		 * boot.
		 */
		if (ifaddr->is_temporary) {
			static bool once;

			LOG_INF("Temporary IPv6 address %s added",
				inet_ntop(AF_INET6, &added_addr, addr_str,
					  sizeof(addr_str) - 1));

			if (!once) {
				k_sem_give(&run_app);
				once = true;
			}
		}
	}

	if (mgmt_event == NET_EVENT_IPV6_ADDR_DEPRECATED) {
		struct in6_addr deprecated_addr;

		memcpy(&deprecated_addr, cb->info, sizeof(struct in6_addr));

		LOG_INF("IPv6 address %s deprecated",
			inet_ntop(AF_INET6, &deprecated_addr, addr_str,
				  sizeof(addr_str) - 1));

		(void)check_our_ipv6_sockets(tx_conf.ipv6.udp.sock,
					     &deprecated_addr);

		if (need_restart) {
			eventfd_write(fds[0].fd, 1);
		}

		return;
	}
}

static void event_handler(struct net_mgmt_event_callback *cb,
			  uint32_t mgmt_event, struct net_if *iface)
{
	if ((mgmt_event & EVENT_MASK) != mgmt_event) {
		return;
	}

	if (want_to_quit) {
		k_sem_give(&run_app);
		want_to_quit = false;
	}

	if (mgmt_event == NET_EVENT_L4_CONNECTED) {
		LOG_INF("Network connected");

		connected = true;
		tx_conf.ipv6.udp.mtu = net_if_get_mtu(iface);

		if (!IS_ENABLED(CONFIG_NET_IPV6_PE)) {
			k_sem_give(&run_app);
		}

		return;
	}

	if (mgmt_event == NET_EVENT_L4_DISCONNECTED) {
		if (connected == false) {
			LOG_INF("Waiting network to be connected");
		} else {
			LOG_INF("Network disconnected");
			connected = false;
		}

		k_sem_reset(&run_app);

		return;
	}
}
static void init_app(void)
{
	LOG_INF(APP_BANNER);

	k_sem_init(&quit_lock, 0, K_SEM_MAX_LIMIT);

	if (IS_ENABLED(CONFIG_NET_CONNECTION_MANAGER)) {
		net_mgmt_init_event_callback(&mgmt_cb,
					     event_handler, EVENT_MASK);
		net_mgmt_add_event_callback(&mgmt_cb);

		conn_mgr_mon_resend_status();
	}

	net_mgmt_init_event_callback(&ipv6_mgmt_cb,
				     ipv6_event_handler, IPV6_EVENT_MASK);
	net_mgmt_add_event_callback(&ipv6_mgmt_cb);

	init_udp();
	init_udp();
}

static void start_client(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	int ret;

	while (true) {
		/* Wait for the connection. */
		//k_sem_take(&run_app, K_FOREVER);

		if (IS_ENABLED(CONFIG_NET_IPV6_PE)) {
			/* Make sure that we have a temporary address */
			k_sleep(K_SECONDS(1));
		}

		//set_red();

		set_leds_off();
		set_green();

		do {
			if (need_restart) {
				/* Close all sockets and get a fresh restart */
				stop_udp_and_tcp();
				need_restart = false;
			}

			ret = start_udp_and_tcp();

			while (connected && (ret == 0)) {
				ret = run_udp_and_tcp();

				if (need_restart) {
					break;
				}
			}
		} while (need_restart);

		stop_udp_and_tcp();
	}
}



















void quit(void)
{
	k_sem_give(&quit_lock);
}

static int cmd_sample_quit(const struct shell *sh,
			  size_t argc, char *argv[])
{
	want_to_quit = true;

	conn_mgr_mon_resend_status();

	quit();

	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sample_commands,
	SHELL_CMD(quit, NULL,
		  "Quit the sample application\n",
		  cmd_sample_quit),
	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(sample, &sample_commands,
		   "Sample application commands", NULL);

int main(void)
{
	init_leds();

	init_app();
 
 	start_periodic_message_timer();
 
	if (!IS_ENABLED(CONFIG_NET_CONNECTION_MANAGER)) {
		/* If the config library has not been configured to start the
		 * app only after we have a connection, then we can start
		 * it right away.
		 */
		k_sem_give(&run_app);
	}

	/* Wait for the connection. */
	k_sem_take(&run_app, K_FOREVER);

	set_red();

	//start_udp_and_tcp();

	start_client(NULL, NULL, NULL);

	k_sem_take(&quit_lock, K_FOREVER);

	set_leds_off();

	if (connected) {
		stop_udp_and_tcp();
	}
	
	return 0;
}
