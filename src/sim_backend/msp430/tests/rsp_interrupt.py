#!/usr/bin/env python3
"""Checks the GDB server's interrupt (Ctrl-C) and that reading a register with read side effects doesn't
trigger them: connects to msp430_run --gdb PORT running gdbtest.hex, as a debugger would."""
import socket
import sys
import time


def packet(s, payload):
    s.sendall(b"$%s#%02x" % (payload.encode(), sum(payload.encode()) & 0xFF))
    return reply(s)


def reply(s):
    data = b""
    while True:
        data += s.recv(4096)
        start = data.find(b"$")
        end = data.find(b"#", start)
        if start >= 0 and end >= 0 and len(data) >= end + 3:
            return data[start + 1:end].decode()


port = int(sys.argv[1])
for _ in range(100):
    try:
        s = socket.create_connection(("127.0.0.1", port))
        break
    except OSError:
        time.sleep(0.05)
print("stop on connect:", packet(s, "?"))
s.sendall(b"$c#63")
time.sleep(0.3)
s.sendall(b"\x03")
print("after ctrl-c:", reply(s))
pc = int.from_bytes(bytes.fromhex(packet(s, "p0")), "little")
print("pc in program:", 0x8000 <= pc < 0x10000)
# SYSRSTIV reads clear the reset cause; peeking twice must show it both times
a = packet(s, "m15e,2")
b = packet(s, "m15e,2")
print("sysrstiv peeks equal:", a == b, a)
print("detach:", packet(s, "D"))
