"""Bounded M105 test using ElegooSlicer's standalone-ok completion rule."""
import argparse
import socket
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--host', required=True, help='Printer MKS Wi-Fi IP address')
args = parser.parse_args()
deadline = time.monotonic() + 8
lines = bytearray()
with socket.create_connection((args.host, 8080), timeout=5) as connection:
    connection.settimeout(0.25)
    connection.sendall(b'M105\n')
    while time.monotonic() < deadline:
        try:
            data = connection.recv(4096)
        except socket.timeout:
            continue
        if not data:
            break
        lines.extend(data)
        while b'\n' in lines:
            line, _, rest = lines.partition(b'\n')
            lines = bytearray(rest)
            text = line.decode('utf-8', 'replace').strip()
            print(repr(text), flush=True)
            if text.lower() == 'ok':
                print('PASS: standalone ok received; MKS test can finish.')
                raise SystemExit(0)
print('FAIL: no standalone ok within 8 seconds. Periodic temperature lines do not count.')
raise SystemExit(1)
