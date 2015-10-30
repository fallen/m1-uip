#include "etherbone.h"

extern struct tcp_socket uip_tcp_socket0;
extern uint8_t uip_tcp_rx_buf0[];
extern uint8_t uip_tcp_tx_buf0[];

unsigned int get_increment(unsigned int width)
{
	switch(width)
	{
		case BYTES_ACCESS_4: return 4;
		case BYTES_ACCESS_2: return 2;
		case BYTES_ACCESS_1: return 1;
		default: printf("ERROR: width != 8/16/32 bits");
			return 0;
	}
}

void do_wishbone_write(unsigned int addr, unsigned int value, unsigned int width)
{
	print_debug("writing addr: 0x%08X\n", addr);
	print_debug("value: 0x%08X\n", value);
	if((width & BYTES_ACCESS_4) == BYTES_ACCESS_4)
	{
		unsigned int *addr_p = (unsigned int *)addr;
		print_debug("32-bit access\n");
		*addr_p = value;
	} else if((width & BYTES_ACCESS_2) == BYTES_ACCESS_2)
	{
		uint16_t *addr_p = (uint16_t *)addr;
		print_debug("16-bit access\n");
		*addr_p = value;
	} else if((width & BYTES_ACCESS_1) == BYTES_ACCESS_1)
	{
		unsigned char *addr_p = (unsigned char *)addr;
		print_debug("8-bit access\n");
		*addr_p = value;
	} else
	{
		printf("ERROR: width != 8/16/32 bits");
	}
}

unsigned int do_wishbone_read(unsigned int addr, unsigned int width)
{
	unsigned int value;
	print_debug("reading addr: 0x%08X\n", addr);
	if((width & BYTES_ACCESS_4) == BYTES_ACCESS_4)
	{
		unsigned int *addr_p = (unsigned int *)addr;
		print_debug("32-bit access\n");
		value = *addr_p;
	} else if((width & BYTES_ACCESS_2) == BYTES_ACCESS_2)
	{
		uint16_t *addr_p = (uint16_t *)addr;
		print_debug("16-bit access\n");
		value = *addr_p;
	} else if((width & BYTES_ACCESS_1) == BYTES_ACCESS_1)
	{
		unsigned char *addr_p = (unsigned char *)addr;
		print_debug("8-bit access\n");
		value = *addr_p;
	} else {
		printf("ERROR: width != 8/16/32 bits");
		value = 0;
	}
	return value;
}

int handle_etherbone_packet(struct tcp_socket *s, const char *rxbuf, int rxlen)
{
	struct etherbone_packet *packet = (struct etherbone_packet *)rxbuf;
	unsigned int addr;
	unsigned int value;
	unsigned int width;
	unsigned int rcount = 0, wcount = 0;

	rcount = packet->record_hdr.rcount;
	wcount = packet->record_hdr.wcount;

	print_debug("Received packet!\n");
	print_debug("magic: 0x%04x\n", packet->magic);
	print_debug("version: 0x%1x\n", packet->version);
	print_debug("addr_size: 0x%1X\n", packet->addr_size);
	print_debug("port_size: 0x%1X\n", packet->port_size);
	print_debug("byte enable: 0x%02X\n", packet->record_hdr.byte_enable);
	print_debug("wcount: 0x%02X\n", wcount);
	print_debug("rcount: 0x%02X\n", rcount);

	if (packet->magic != 0x4e6f)
	{
		printf("Wrong magic number : should be 0x4e6f but is 0x%04x", packet->magic);
		return 0;
	}

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

	if(rcount > 0 && wcount > 0)
	{
		printf("ERROR: only 1 read OR 1 read per packet supported\n");
		return 0;
	}

	if(wcount > 0)
	{
		unsigned int w, inc;
		print_debug("Writing: \n");
		addr = packet->record_hdr.base_write_addr;
		width = packet->record_hdr.byte_enable;
		inc = get_increment(width);
		for(w = 0; w < wcount; w++)
		{
			value = packet->record[w].write_value;
			do_wishbone_write(addr + w * inc, value, width);
		}
		return 0;
	}

	if(rcount > 0)
	{
		unsigned int val, r;
		struct etherbone_packet *tx_packet = (struct etherbone_packet *)uip_tcp_tx_buf0;
		print_debug("Reading: ");
		width = packet->record_hdr.byte_enable;
		for(r = 0; r < rcount; r++)
		{
			addr = packet->record[r].read_addr;
			val = do_wishbone_read(addr, width);
			tx_packet->record[r].write_value = val;
			print_debug("READ result: 0x%08x\n", val);
		}
		tx_packet->magic = 0x4e6f;
		tx_packet->version = 1;
		tx_packet->nr = 1;
		tx_packet->pr = 0;
		tx_packet->pf = 0;
		tx_packet->addr_size = 4; // 32 bits
		tx_packet->port_size = 4; // 32 bits
		tx_packet->record_hdr.wcount = rcount;
		tx_packet->record_hdr.rcount = 0;
		tx_packet->record_hdr.base_write_addr = packet->record_hdr.base_ret_addr;
		print_debug("sending size: %d\n", sizeof(*tx_packet));
		tcp_socket_send(&uip_tcp_socket0, uip_tcp_tx_buf0, sizeof(*tx_packet) + rcount*sizeof(struct etherbone_record));
	}

	return 0;
}
