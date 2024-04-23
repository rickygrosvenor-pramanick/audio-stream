# This short Python sript ensures that your stream_request_response
# is able to handle the stream request packet being broken into
# two pieces.
#

import socket
import time

def main(host, port):
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect((host, port))
    sock.send(b"STREAM\r\n\x00\x00")
    time.sleep(1)
    sock.send(b"\x00\x01")
    data = int.from_bytes(sock.recv(4), "big")
    print("Receiving file of size %d bytes\n"%data)
    sock.close()

if __name__ == "__main__":
    import sys
    if len(sys.argv) != 3:
        print("usage: %s host port"%sys.argv[0])
        sys.exit(1)
    main(sys.argv[1], int(sys.argv[2]))
