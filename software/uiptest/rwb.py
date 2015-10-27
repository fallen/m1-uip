import argparse
import socket

from liteeth.software.etherbone import *


def get_argparser():
    parser = argparse.ArgumentParser(description="Remote wishbone access tool")
    parser.add_argument("-r", "--read", nargs=2, metavar=("ADDR", "WIDTH"),
                        help="Read address on wishbone bus")
    parser.add_argument("-w", "--write", nargs=3,
                        metavar=("ADDR", "VALUE", "WIDTH"),
                        help="Write some value to an address on wishbone bus")
    parser.add_argument("--host", required=True,
                        help="Host to connect to")
    parser.add_argument("-p", "--port", default=80,
                        help="Port to connect to")
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


def main():
    args = get_argparser().parse_args()

    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((args.host, args.port))

    if args.read:
        packet = EtherbonePacket()
        record = EtherboneRecord()
        reads = EtherboneReads(addrs=[string_to_int(args.read[0])])
        record.writes = None
        record.reads = reads
        record.bca = 0
        record.rca = 0
        record.rff = 0
        record.cyc = 0
        record.wca = 0
        record.wff = 0
        record.byte_enable = make_byte_enable(int(args.read[1]))
        record.wcount = 0
        record.rcount = 1
        packet.records = [record]
        packet.encode()
        s.send(bytes(packet))
        answer = s.recv(1024)
        answer_packet = EtherbonePacket(init=[b for b in answer])
        answer_packet.decode()
        print("0x{:08x}"
              .format(answer_packet.records[0].writes.writes[0].data))
    elif args.write:
        packet = EtherbonePacket()
        record = EtherboneRecord()
        writes = EtherboneWrites(base_addr=string_to_int(args.write[0]),
                                 datas=[string_to_int(args.write[1])])
        record.writes = writes
        record.reads = None
        record.bca = 0
        record.rca = 0
        record.rff = 0
        record.cyc = 0
        record.wca = 0
        record.wff = 0
        record.byte_enable = make_byte_enable(int(args.write[2]))
        record.wcount = 1
        record.rcount = 0
        packet.records = [record]
        packet.encode()
        s.send(bytes(packet))


if __name__ == "__main__":
    main()
