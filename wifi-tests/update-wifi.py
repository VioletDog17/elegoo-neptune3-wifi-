"""Explicit manual OTA of this project's ESP8266 host. No automatic flashing."""
import argparse
import json
from pathlib import Path
from urllib.request import Request, urlopen

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("host", help="Printer IP or http://IP")
parser.add_argument("binary", type=Path)
parser.add_argument("--install", action="store_true", help="Actually send the firmware")
args = parser.parse_args()
base = args.host.rstrip("/")
if not base.startswith("http://"):
    base = "http://" + base
def get(path):
    with urlopen(base + path, timeout=15) as response:
        return json.load(response)
data = args.binary.read_bytes()
if not data or data[0] != 0xE9:
    raise SystemExit("Not an ESP8266 firmware image")
info, status = get("/api/info"), get("/api/status")
printer = status.get("printer", {})
if not status.get("online") or printer.get("state") != "idle" or printer.get("busy") or printer.get("nozzle_target") or printer.get("bed_target"):
    raise SystemExit("Printer must be online, idle, with both heater targets zero")
print(f"Target: {base}, module {info['firmware']}; image {len(data)} bytes")
if not args.install:
    raise SystemExit("Read-only check complete. Add --install to update and reboot the Wi-Fi module.")
request = Request(base + "/api/update", data=data, headers={"Content-Type": "application/octet-stream", "X-Neptune-Session": info["session"]}, method="POST")
with urlopen(request, timeout=180) as response:
    print(response.read().decode())
