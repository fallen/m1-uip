struct etherbone_record {
    union {
        uint16_t base_write_addr;
        uint16_t base_ret_addr;
    };
    union {
        uint16_t write_value;
        uint16_t read_addr;
    };
};

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
} __attribute__((packed, aligned(8)));

struct etherbone_packet {
    uint16_t magic;
    unsigned int version: 4;
    unsigned int reserved: 1;
    unsigned int nr: 1;
    unsigned int pr: 1;
    unsigned int pf: 1;
    unsigned int addr_size: 4;
    unsigned int port_size: 4;

    struct etherbone_record_header record_hdr;
    struct etherbone_record record;
} __attribute__((packed, aligned(4)));
