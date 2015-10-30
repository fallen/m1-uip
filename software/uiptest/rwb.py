import argparse
import socket
import struct

from liteeth.software.etherbone import *

ETHERBONE_HEADER_SIZE = 12


def get_argparser():
    parser = argparse.ArgumentParser(description="Remote wishbone access tool")
    parser.add_argument("-r", "--read", nargs=2, metavar=("ADDR", "WIDTH"),
                        help="Read address on wishbone bus")
    parser.add_argument("-w", "--write", nargs=3,
                        metavar=("ADDR", "VALUE", "WIDTH"),
                        help="Write some value to an address on wishbone bus")
    parser.add_argument("-n", "--num", type=int, default=1,
                        help="Number of sequential reads")
    parser.add_argument("--host", required=True,
                        help="Host to connect to")
    parser.add_argument("-p", "--port", default=80,
                        help="Port to connect to")
    parser.add_argument("-u", "--upload", nargs=2, metavar=("ADDR", "FILENAME"),
                        help="Write a file content to memory")
    parser.add_argument("-d", "--download", nargs=3, metavar=("ADDR", "SIZE", "FILENAME"),
                        help="Write a file content to memory")
    return parser


def string_to_int(mystr):
    if mystr.upper().startswith("0X"):
        return int(mystr, 16)
    elif mystr.startswith("0"):
        return int(mystr, 8)
    else:
        return int(mystr)


def make_byte_enable(width):
    if width == 32:
        return 1+2+4+8
    elif width == 16:
        return 1+2
    elif width == 8:
        return 1
    else:
        raise ValueError("Width is supposed to be either 32 or 16 or 8.")


def receive_etherbone_packet(s):
    packet = bytes()
    while len(packet) < ETHERBONE_HEADER_SIZE:
        packet += s.recv(ETHERBONE_HEADER_SIZE - len(packet))

    wcount, rcount = struct.unpack(">BB", packet[-2:])

    counts = wcount + rcount

    packet_size = ETHERBONE_HEADER_SIZE + 4*(counts + 1)
    while len(packet) < packet_size:
        packet += s.recv(packet_size - len(packet))

    return packet


def send_reads(s, addresses, width):
    packet = EtherbonePacket()
    record = EtherboneRecord()
    reads = EtherboneReads(addrs=addresses)
    record.writes = None
    record.reads = reads
    record.bca = 0
    record.rca = 0
    record.rff = 0
    record.cyc = 0
    record.wca = 0
    record.wff = 0
    record.byte_enable = make_byte_enable(width)
    record.wcount = 0
    record.rcount = len(addresses)
    packet.records = [record]
    packet.encode()
    s.send(bytes(packet))

    answer = receive_etherbone_packet(s)
    answer_packet = EtherbonePacket(init=[b for b in answer])
    answer_packet.decode()
    return answer_packet


def send_writes(s, address, width, values):
    packet = EtherbonePacket()
    record = EtherboneRecord()
    writes = EtherboneWrites(base_addr=address,
                             datas=values)
    record.writes = writes
    record.reads = None
    record.bca = 0
    record.rca = 0
    record.rff = 0
    record.cyc = 0
    record.wca = 0
    record.wff = 0
    record.byte_enable = make_byte_enable(width)
    record.wcount = len(values)
    record.rcount = 0
    packet.records = [record]
    packet.encode()
    s.send(bytes(packet))


def main():
    args = get_argparser().parse_args()

    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((args.host, args.port))

    if args.read:
        addr, width = args.read
        addr = string_to_int(addr)
        width = int(width)
        addresses = range(addr, addr + args.num * (width//8), width//8)
        answer = send_reads(s, addresses, width)
        for w, addr in zip(answer.records[0].writes.writes, addresses):
            print("0x{:08x}: 0x{:08x}"
                  .format(addr, w.data))
    elif args.write:
        addr, value, width = args.write
        addr = string_to_int(addr)
        value = string_to_int(value)
        width = int(width)
        send_writes(s, addr, width, [value])
    elif args.upload:
        addr, filename = args.upload
        addr = string_to_int(addr)
        with open(filename, "rb") as file:
            content = file.read()
            size = len(content)
            while size >= 4:
                num_writes = min(100, size//4)
                values = struct.unpack(">"+"I"*num_writes, content[:4*num_writes])
                send_writes(s, addr, 32, values)
                content = content[4*num_writes:]
                size -= 4*num_writes
                addr += 4*num_writes
            for b in content:
                send_writes(s, addr, 8, [b])
                addr += 1
    elif args.download:
        addr, size, filename = args.download
        size = string_to_int(size)
        addr = string_to_int(addr)
        with open(filename, "wb") as file:
            while size >= 4:
                num_reads = min(100, size//4)
                addresses = range(addr, addr + 4*num_reads, 4)
                p = send_reads(s, addresses, 32)
                for w in p.records[0].writes.writes:
                    file.write(struct.pack(">I", w.data))
                addr += 4*num_reads
                size -= 4*num_reads
            for i in range(size):
                p = send_reads(s, [addr], 8)
                file.write(p.records[0].writes.writes[0].data)
                addr += 1

if __name__ == "__main__":
    main()
