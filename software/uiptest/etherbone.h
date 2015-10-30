#include "contiki.h"
#include "contiki-net.h"

//#define ETHERBONE_DEBUG

#ifdef ETHERBONE_DEBUG
	#define print_debug(...) printf(__VA_ARGS__)
#else
	#define print_debug(...) {}
#endif

#define BYTES_ACCESS_1  0x1
#define BYTES_ACCESS_2  0x3
#define BYTES_ACCESS_4  0xf

struct etherbone_record {
    union {
        uint32_t write_value;
        uint32_t read_addr;
    };
} __attribute__((packed));

struct etherbone_record_header {
    unsigned int bca: 1;
    unsigned int rca: 1;
    unsigned int rff: 1;
    unsigned int reserved: 1;
    unsigned int cyc: 1;
    unsigned int wca: 1;
    unsigned int wff: 1;
    unsigned int reserved2: 1;
    unsigned char byte_enable;
    unsigned char wcount;
    unsigned char rcount;
    union {
        uint32_t base_write_addr;
        uint32_t base_ret_addr;
    };
} __attribute__((packed));

struct etherbone_packet {
    uint16_t magic;
    unsigned int version: 4;
    unsigned int reserved: 1;
    unsigned int nr: 1;
    unsigned int pr: 1;
    unsigned int pf: 1;
    unsigned int addr_size: 4;
    unsigned int port_size: 4;
    uint32_t padding;

    struct etherbone_record_header record_hdr;
    struct etherbone_record record[];
} __attribute__((packed, aligned(8)));

unsigned int get_increment(unsigned int width);
void do_wishbone_write(unsigned int addr, unsigned int value, unsigned int width);
unsigned int do_wishbone_read(unsigned int addr, unsigned int width);
int handle_etherbone_packet(struct tcp_socket *s, unsigned char *rxbuf);
