#!/usr/bin/env python3

import socket
import time

msg_size = 1024
test_size = 1024*1024

for port in [80]:
	print("Testing loopback on port {}...".format(str(port)))
	errors = 0
	s = socket.socket(socket.AF_INET,socket.SOCK_STREAM)
	s.connect(("192.168.0.42", port))
	start = time.time()
	for i in range(test_size//msg_size):
		transmit_data = [i%256 for i in range(msg_size)]
		s.send(bytes(transmit_data))
		receive_data = list(s.recv(4096))
		for t, r in zip(transmit_data, receive_data):
			if t != r:
				errors += 1
	end = time.time()
	if (end-start) != 0:
		speed = "{:3.2f} KBps".format((test_size/(end-start))/1000)
	else:
		speed = "Unable to compute speed"
	print("errors: {} / {}".format(errors, speed))
	s.close()
