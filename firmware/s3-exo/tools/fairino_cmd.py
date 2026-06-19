#!/usr/bin/env python3
"""Fairino Robot UDP Command Client
Sends commands to ESP32 (192.168.58.100:20008) which relays to robot (192.168.58.2:20007)

Usage:
  python fairino_cmd.py status
  python fairino_cmd.py 'servo j1wave 1 5'
  python fairino_cmd.py 'servo end'
"""

import socket, sys

ESP32 = ('192.168.58.100', 20008)

def send_cmd(msg):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(5.0)
    sock.bind(('0.0.0.0', 0))
    sock.sendto(msg.encode(), ESP32)
    try:
        data, _ = sock.recvfrom(4096)
        print(data.decode().strip())
    except socket.timeout:
        print('(timeout)')
    sock.close()

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(0)
    send_cmd(' '.join(sys.argv[1:]))
