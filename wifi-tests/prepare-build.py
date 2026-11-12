"""Restore stock build metadata shipped as an embedded archive by ELEGOO."""
from pathlib import Path
from zipfile import ZipFile

project = (Path(__file__).resolve().parents[1] / "neptune3pro-main/Firmware/Mainboard/"
           "Marlin-2.1.1_TJC_029/Marlin-2.1.1_TJC_029")
destination = project / "buildroot/share"
with ZipFile(destination / "PlatformIO.zip") as archive:
    for item in archive.infolist():
        target = (destination / item.filename).resolve()
        if not target.is_relative_to(destination.resolve()):
            raise ValueError(f"Archive member escapes destination: {item.filename}")
    archive.extractall(destination)
print("Stock PlatformIO board definitions, variants and build scripts restored.")
