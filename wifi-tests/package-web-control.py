"""Package the three previously compiled firmware images. Does not flash."""
from pathlib import Path
from datetime import datetime, timezone
import hashlib
import json
import shutil
import zipfile

root = Path(__file__).resolve().parents[1]
out = root / "firmware/v0.2.0-dev"
sources = {
    "mainboard/ZNP_ROBIN_NANO.bin": root / "dist/NEPTUNE3_MKS_WIFI/ZNP_ROBIN_NANO.bin",
    "screen/NEPTUNE3_WIFI_CONTROL.tft": root / "dist/NEPTUNE3_WIFI_CONTROL/screen/NEPTUNE3_WIFI_CONTROL.tft",
    "wifi/MksWifi.bin": root / "esp8266/.pio/build/mks_robin_wifi/firmware.bin",
}
for target, source in sources.items():
    assert source.is_file(), f"Build first: {source}"
    destination = out / target
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(source, destination)
for name in ("WEB-CONTROL-RU.md", "INSTALL-RU.md", "SCREENS-RU.md"):
    (out / "docs").mkdir(exist_ok=True)
    text = (root / "docs" / name).read_text(encoding="utf-8")
    (out / "docs" / name).write_bytes(("\n".join(line.rstrip() for line in text.splitlines()) + "\n").encode("utf-8"))
shutil.copytree(root / "docs/screenshots", out / "docs/screenshots", dirs_exist_ok=True)
readme = """# Neptune 3 Pro Wi-Fi Control · v0.2.0-dev

Экспериментальный комплект. Частично проверен на принтере: OTA, статус, выключение нагрева, загрузки 43/217141 байт и русское имя в списке SD. Полная проверка управления и печати ещё предстоит.
Создано с помощью ChatGPT/Codex; автор аппаратных проверок — VioletDog.

Цели: Neptune 3 Pro, ZNP Robin Nano_DW V2.2 (stock marlin_MKS_F401RC), MKS Robin WIFI V1.1 / ESP8266 4 MB DOUT, TJC4827X243_011 272×480.
Не применять к Plus/Max или другим платам/дисплеям.

- mainboard/ZNP_ROBIN_NANO.bin — microSD основной платы.
- screen/NEPTUNE3_WIFI_CONTROL.tft — microSD внутри дисплея.
- wifi/MksWifi.bin — поле **wifi firmware** старой веб-страницы MKS, не web view.

[Полная установка и чеклист](docs/WEB-CONTROL-RU.md) · [Галерея](docs/SCREENS-RU.md).
BIN основной платы нельзя отправлять в поле обновления Wi-Fi. Новая панель встроена в ESP BIN, ПК-мост не нужен.
Старый комплект v0.1 сохранён отдельно. Полного дампа установленного устройства нет.
"""
(out / "README.md").write_bytes(readme.encode("utf-8"))
(out / "INSTALL-RU.md").write_bytes("Читайте [пошаговую инструкцию для трёх компонентов](docs/WEB-CONTROL-RU.md).\n".encode("utf-8"))
def digest(path):
    data = path.read_bytes()
    return {"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}
manifest = {
    "version": "0.2.0-dev", "hardware_tested": False,
    "created_at": datetime.now(timezone.utc).isoformat(),
    "mainboard_environment": "NEPTUNE3_MKS_WIFI",
    "mainboard_mcu_build": "stock marlin_MKS_F401RC, 84MHz, flash offset 0x8000",
    "display_model": "TJC4827X243_011", "display_orientation": "272x480 portrait UTF-8",
    "screen_compiler": "USART HMI 1.68.2", "screen_compile_errors": 0, "screen_compile_warnings": 0,
    "screen_source_sha256": digest(root / "neptune3pro-main/Firmware/LCD/NEPTUNE3_WIFI_CONTROL.HMI")["sha256"],
    "wifi_target": "MKS Robin WIFI V1.1 / ESP8266 / 4MB flash / DOUT / eagle.flash.4m3m.ld",
    "wifi_platform": "espressif8266@4.2.1, Arduino 3.1.2",
    "wifi_firmware_revision": "0.2.0-dev-uart2",
    "hardware_validation": {
        "date": "2026-11-13", "wifi_ota": True, "live_status": True,
        "cooldown": True, "upload_saved_bytes": [43, 217141],
        "cyrillic_filename_and_size": True, "test_print_started": False,
        "slicer_upload_user_confirmed": "2026-11-14",
        "remaining": ["motion", "print/pause/resume", "wifi_reconfiguration"],
    },
    "validation": ["seven native C++ test suites PASS", "mainboard and ESP8266 builds PASS", "TJC simulator: Cyrillic filename visible, refresh VP 0x2203 verified", "HMI comparison: only file1 and printpause changed", "headless Chrome: desktop/mobile, Wi-Fi dialog, filename/filter, mocked controls and offline locks PASS"],
    "files": {str(p.relative_to(out)).replace("\\", "/"): digest(p) for p in sorted(out.rglob("*")) if p.is_file() and p.name not in ("manifest.json", "SHA256SUMS.txt") and p.suffix != ".zip"},
}
(out / "manifest.json").write_bytes((json.dumps(manifest, ensure_ascii=False, indent=2) + "\n").encode("utf-8"))
paths = [p for p in sorted(out.rglob("*")) if p.is_file() and p.name != "SHA256SUMS.txt" and p.suffix != ".zip"]
checks = "".join(digest(p)["sha256"] + "  " + p.relative_to(out).as_posix() + "\n" for p in paths)
(out / "SHA256SUMS.txt").write_bytes(checks.encode("utf-8"))
archive = out / "Neptune3-WiFi-Control-v0.2.0-dev-uart2.zip"
with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as package:
    for path in paths + [out / "SHA256SUMS.txt"]:
        package.write(path, path.relative_to(out).as_posix())
print(archive)
print(json.dumps({name: digest(out / name) for name in sources}, indent=2))
