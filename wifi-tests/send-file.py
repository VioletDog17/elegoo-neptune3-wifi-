"""Send a G-code file to MKS SD storage. Never sends selection/start commands."""
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


def query(host, command, seconds=2):
    with socket.create_connection((host, 8080), timeout=5) as connection:
        connection.settimeout(0.25)
        connection.sendall((command + '\n').encode('utf-8'))
        result = bytearray()
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            try:
                data = connection.recv(4096)
                if not data:
                    break
                result.extend(data)
            except socket.timeout:
                continue
        return result.decode('utf-8', 'replace')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('file', type=Path)
    parser.add_argument('--host', required=True, help='Printer MKS Wi-Fi IP address')
    parser.add_argument('--timeout', type=int, default=1800, help='Socket transfer timeout in seconds for slow MKS uploads')
    args = parser.parse_args()
    file = args.file.resolve(strict=True)
    if (not 1 <= len(file.name.encode('utf-8')) <= 90 or file.name.startswith('.') or file.name.endswith(' ')
            or '..' in file.name or any(ord(c) < 160 and not re.fullmatch(r'[A-Za-z0-9_() .-]', c) for c in file.name)
            or file.suffix.lower() not in ('.gcode', '.gco', '.gc', '.g')):
        raise ValueError('Use a printable UTF-8 G-code filename, at most 90 bytes, without paths or control characters.')
    size = file.stat().st_size
    if not size:
        raise ValueError('Empty file')
    before = query(args.host, ' M998')
    if not re.search(r'M998 (NONE|SAVED) ', before):
        raise RuntimeError('Upload firmware not ready: ' + before)
    state = query(args.host, 'M997')
    if 'M997 IDLE' not in state:
        raise RuntimeError('Printer not idle: ' + state)
    digest = hashlib.sha256(file.read_bytes()).hexdigest()
    print(f'Uploading {file.name}: {size} bytes; no print start', flush=True)
    session = requests.Session()
    session.trust_env = False
    started = time.monotonic()
    with file.open('rb') as body:
        class ProgressFile:
            def __init__(self, source):
                self.source = source
                self.reported = 0
            def __getattr__(self, name):
                return getattr(self.source, name)
            def read(self, count=-1):
                chunk = self.source.read(count)
                position = self.source.tell()
                if position - self.reported >= 1048576 or (not chunk and position == size):
                    print(f'Transfer stream: {position}/{size} bytes ({100*position/size:.1f}%), elapsed {time.monotonic()-started:.0f}s', flush=True)
                    self.reported = position
                return chunk
        response = session.post('http://' + args.host + '/upload', params={'X-Filename': file.name},
                                data=ProgressFile(body), headers={'Content-Type': 'application/octet-stream'}, timeout=(args.timeout, args.timeout))
    response.raise_for_status()
    if response.json().get('err', 0) != 0:
        raise RuntimeError('Module upload error: ' + response.text)
    deadline = time.monotonic() + 30
    time.sleep(2)
    saved = None
    last = ''
    while time.monotonic() < deadline:
        last = query(args.host, ' M998')
        match = re.search(r'M998 SAVED (\d+) ([A-Z0-9]+\.GCO)', last)
        if match and int(match[1]) == size:
            # Never mistake an unchanged previous completion for this upload.
            if match.group(0) not in before:
                saved = match[2]
                break
        time.sleep(0.5)
    if not saved:
        raise RuntimeError('No new SD completion matching the file size: ' + last)
    result = dict(source=str(file), bytes=size, sha256=digest, sd_name=saved,
                  seconds=round(time.monotonic()-started, 2), print_started=False)
    (repo / 'last-file-upload.log').write_text(json.dumps(result, indent=2), encoding='utf-8')
    print(json.dumps(result, indent=2), flush=True)


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, RuntimeError, requests.RequestException) as error:
        print('FAIL:', error, file=sys.stderr)
        sys.exit(1)
