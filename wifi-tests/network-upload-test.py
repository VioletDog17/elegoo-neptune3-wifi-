"""MKS integration test. Default: upload only, no motion/heating/print start."""
import argparse
import hashlib
import json
import re
import socket
import sys
import time
from pathlib import Path

repo = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(repo / '.tools/pio'))
import requests

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--host', required=True, help='Printer MKS Wi-Fi IP address')
parser.add_argument('--bytes', type=int, default=2030, help='Test length, 128..1048576 bytes; default tests two complete blocks')
args = parser.parse_args()
if not 128 <= args.bytes <= 1048576:
    parser.error('--bytes must be between 128 and 1048576')


def query(command):
    with socket.create_connection((args.host, 8080), timeout=5) as connection:
        connection.settimeout(0.25)
        connection.sendall((command + '\n').encode('ascii'))
        result = bytearray()
        deadline = time.monotonic() + 2
        while time.monotonic() < deadline:
            try:
                data = connection.recv(4096)
                if not data:
                    break
                result.extend(data)
            except socket.timeout:
                continue
        return result.decode('utf-8', 'replace')


try:
    # Stock ESP consumes lines starting exactly with M998 for ROBIN devices.
    # A leading space bypasses that shortcut; our command parser trims it.
    reply = query(' M998')
    if not re.search(r'M998 (NONE|SAVED|RECEIVING) ', reply):
        raise RuntimeError('Upload firmware not confirmed by M998. Flash NEPTUNE3_MKS_WIFI first. Reply: ' + reply)
    # Exactly two full MKS data blocks; requires an empty LAST fragment.
    # Comments + display message only. This script never sends M23 or M24.
    fixture = b'; MKS upload boundary test. No motion or heater commands.\nM117 WIFI_UPLOAD_OK\n'
    line = b'; Test padding - ABCDEFGHIJKLMNOPQRSTUVWXYZ 0123456789\n'
    while len(fixture) + len(line) < args.bytes:
        fixture += line
    remaining = args.bytes - len(fixture)
    fixture += b'\n' if remaining == 1 else b';' + b'x' * (remaining - 2) + b'\n'
    assert len(fixture) == args.bytes
    path = repo / ('wifi-tests/UPLOAD_TEST.GCO' if args.bytes == 2030 else 'wifi-tests/UPLOAD_LARGE_TEST.gcode')
    path.write_bytes(fixture)
    session = requests.Session()
    session.trust_env = False
    response = session.post('http://' + args.host + '/upload', params={'X-Filename': path.name},
                            data=fixture, headers={'Content-Type': 'application/octet-stream'}, timeout=(5, 60))
    response.raise_for_status()
    body = response.json()
    if body.get('err', 0) != 0:
        raise RuntimeError('Module rejected upload: ' + json.dumps(body))
    # HTTP success alone is insufficient: stock ESP may return it before SD close.
    time.sleep(2)
    deadline = time.monotonic() + 20
    saved = None
    while time.monotonic() < deadline:
        reply = query(' M998')
        match = re.search(r'M998 SAVED (\d+) ([A-Z0-9]+\.GCO)', reply)
        if match and int(match[1]) == len(fixture):
            saved = match[2]
            break
        time.sleep(0.5)
    if not saved:
        raise RuntimeError('Module accepted HTTP data, but SD finalization was not confirmed. Last reply: ' + reply)
    print('PASS: module accepted upload; printer confirmed SAVED, ' + str(len(fixture)) + ' bytes, SD name ' + saved)
    print('Reference file: ' + str(path))
    print('SHA256 reference: ' + hashlib.sha256(fixture).hexdigest())
    print('No M23/M24 sent. For full data verification, copy ' + saved + ' from SD and compare it to the reference.')
except (OSError, ValueError, RuntimeError, requests.RequestException) as error:
    print('FAIL:', error, file=sys.stderr)
    sys.exit(1)
