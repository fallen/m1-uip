#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <irq.h>
#include <uart.h>
#include <time.h>
#include <generated/csr.h>
#include <hw/flags.h>
#include <console.h>
#include <system.h>

#include "contiki.h"
#include "contiki-net.h"
#include "liteethmac-drv.h"
#include "etherbone.h"

//#define UIP_DEBUG

#define UIP_TCP_SERVER_PORT0 80
#define UIP_TCP_BUFFER_SIZE_RX 400 /* XXX */
#define UIP_TCP_BUFFER_SIZE_TX 400 /* XXX */

#define BYTES_ACCESS_1  0x1
#define BYTES_ACCESS_2  0x3
#define BYTES_ACCESS_4  0x7

static int uip_periodic_event;
static int uip_periodic_period;

static int uip_arp_event;
static int uip_arp_period;

static struct tcp_socket uip_tcp_socket0;
static uint8_t uip_tcp_rx_buf0[UIP_TCP_BUFFER_SIZE_RX];
static uint8_t uip_tcp_tx_buf0[UIP_TCP_BUFFER_SIZE_TX];

static const unsigned char macadr[6] = {0x10, 0xe2, 0xd5, 0x00, 0x00, 0x00};
static const unsigned char ipadr[4] = {192, 168, 24, 142};

static void do_wishbone_write(unsigned int addr, unsigned int value, unsigned int width)
{
	if(width & BYTES_ACCESS_4)
	{
		unsigned int *addr_p = (unsigned int *)addr;
		*addr_p = value;
	} else if(width & BYTES_ACCESS_2)
	{
		uint16_t *addr_p = (uint16_t *)addr;
		*addr_p = value;
	} else if(width & BYTES_ACCESS_1)
	{
		unsigned char *addr_p = (unsigned char *)addr;
		*addr_p = value;
	} else
	{
		printf("ERROR: width != 8/16/32 bits");
	}
}

static unsigned int do_wishbone_read(unsigned int addr, unsigned int width)
{
	unsigned int value;

	if(width & BYTES_ACCESS_4)
	{
		unsigned int *addr_p = (unsigned int *)addr;
		value = *addr_p;
	} else if(width & BYTES_ACCESS_2)
	{
		uint16_t *addr_p = (uint16_t *)addr;
		value = *addr_p;
	} else if(width & BYTES_ACCESS_1)
	{
		unsigned char *addr_p = (unsigned char *)addr;
		value = *addr_p;
	}
	printf("ERROR: width != 8/16/32 bits");
	return value;
}

static char *readstr(void)
{
	char c[2];
	static char s[64];
	static int ptr = 0;

	if(readchar_nonblock()) {
		c[0] = readchar();
		c[1] = 0;
		switch(c[0]) {
			case 0x7f:
			case 0x08:
				if(ptr > 0) {
					ptr--;
					putsnonl("\x08 \x08");
				}
				break;
			case 0x07:
				break;
			case '\r':
			case '\n':
				s[ptr] = 0x00;
				putsnonl("\n");
				ptr = 0;
				return s;
			default:
				if(ptr >= (sizeof(s) - 1))
					break;
				putsnonl(c);
				s[ptr] = c[0];
				ptr++;
				break;
		}
	}
	return NULL;
}

static void console_service(void)
{
	char *str;

	str = readstr();
	if(str == NULL) return;

//	if(strcmp(str, "reboot") == 0) asm("call r0");
}

void uip_log(char *msg){
#ifdef UIP_DEBUG
	puts(msg);
#endif
}

static int uip_tcp_rx0(struct tcp_socket *s, void *ptr, const char *rxbuf, int rxlen)
{
	struct etherbone_packet *packet = (struct etherbone_packet *)rxbuf;
	unsigned int addr;
	unsigned int value;
	unsigned int width;

	/* loopback */
//	printf("%s", rxbuf);
	printf("magic: 0x%08x\n", packet->magic);
	printf("version: 0x%1x\n", packet->version);
	if(packet->addr_size != 4)
	{
		printf("ERROR: bus address width != 32 bits\n");
		return 0;
	}

	if(packet->port_size != 4)
	{
		printf("ERROR: bus data width != 32 bits\n");
		return 0;
	}

	if(packet->record_hdr.rcount > 0 && packet->record_hdr.wcount > 0)
	{
		printf("ERROR: only 1 read OR 1 read per packet supported\n");
		return 0;
	}

	if(packet->record_hdr.wcount > 0)
	{
		printf("Writing: ");
		if(packet->record_hdr.wcount > 1)
			printf("ERROR: only 1 write per packet supported\n");
		addr = packet->record.base_write_addr;
		value = packet->record.write_value;
		width = packet->record_hdr.byte_enable;

		do_wishbone_write(addr, value, width);
	}

	if(packet->record_hdr.rcount > 0)
	{
		unsigned int val;
		printf("Reading: ");
		if (packet->record_hdr.rcount > 1)
			printf("ERROR: only 1 read per packet supported\n");
		addr = packet->record.read_addr;
		width = packet->record_hdr.byte_enable;

		val = do_wishbone_read(addr, width);
	}
//	tcp_socket_send_str(&uip_tcp_socket0, rxbuf);
	return 0;
}

static void uip_tcp_event0(struct tcp_socket *s, void *ptr, tcp_socket_event_t event)
{
	/* XXX do something? */
#ifdef UIP_DEBUG
	printf("uip_tcp_event0 %d\n", event);
#endif
}

static void uip_service(void)
{
	int i;
	struct uip_eth_hdr *buf = (struct uip_eth_hdr *)&uip_buf[0];

	etimer_request_poll();
	process_run();

	uip_len = liteethmac_poll();
	if(uip_len > 0) {
		if(buf->type == uip_htons(UIP_ETHTYPE_IP)) {
			uip_arp_ipin();
			uip_input();
			if(uip_len > 0) {
				uip_arp_out();
				liteethmac_send();
			}
		} else if(buf->type == uip_htons(UIP_ETHTYPE_ARP)) {
			uip_arp_arpin();
			if(uip_len > 0)
				liteethmac_send();
		}
	} else if (elapsed(&uip_periodic_event, uip_periodic_period)) {
		for(i = 0; i < UIP_CONNS; i++) {
			uip_periodic(i);

			if(uip_len > 0) {
				uip_arp_out();
				liteethmac_send();
			}
		}
	}
	if (elapsed(&uip_arp_event, uip_arp_period)) {
		uip_arp_timer();
	}
}

int main(void)
{
	int i;
	uip_ipaddr_t ipaddr;

	irq_setmask(0);
	irq_setie(1);
	uart_init();

	puts("uIP testing software built "__DATE__" "__TIME__"\n");

	time_init();
	clock_init();
	process_init();
	process_start(&etimer_process, NULL);

	/* XXX what are the expected periods? */
	uip_periodic_period = identifier_frequency_read();
	uip_arp_period = identifier_frequency_read();

	liteethmac_init();
	uip_init();

	for (i=0; i<6; i++) uip_lladdr.addr[i] = macadr[i];
	uip_ipaddr(&ipaddr, ipadr[0], ipadr[1], ipadr[2], ipadr[3]);
	uip_sethostaddr(&ipaddr);

	tcp_socket_register(&uip_tcp_socket0, NULL,
		uip_tcp_rx_buf0, UIP_TCP_BUFFER_SIZE_RX,
		uip_tcp_tx_buf0, UIP_TCP_BUFFER_SIZE_TX,
		(tcp_socket_data_callback_t) uip_tcp_rx0, uip_tcp_event0);
	tcp_socket_listen(&uip_tcp_socket0, UIP_TCP_SERVER_PORT0);
	printf("Listening on %d\n", UIP_TCP_SERVER_PORT0);

	while(1) {
		console_service();
		uip_service();
	}

	return 0;
}
