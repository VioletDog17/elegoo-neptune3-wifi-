"""Render code-native TJC header assets. Requires Pillow and Windows Arial Bold."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

SCALE = 4
OUT = Path(__file__).with_name("assets")
OUT.mkdir(exist_ok=True)
font = ImageFont.truetype("C:/Windows/Fonts/arialbd.ttf", 18 * SCALE)
for connected in (False, True):
    image = Image.new("RGB", (272 * SCALE, 69 * SCALE), (46, 54, 75))
    draw = ImageDraw.Draw(image)
    draw.text((14 * SCALE, 21 * SCALE), "NEPTUNE 3 PRO WIFI MOD",
              font=font, fill="white", anchor="lt")
    draw.rectangle((15 * SCALE, 49 * SCALE, 30 * SCALE, 50 * SCALE),
                   fill=(0, 224, 240))
    color = (255, 255, 255) if connected else (0, 0, 0)
    for radius in (10, 6):
        draw.arc(((246-radius)*SCALE, (62-radius)*SCALE,
                  (246+radius)*SCALE, (62+radius)*SCALE),
                 225, 315, fill=color, width=SCALE+2)
    draw.ellipse((245*SCALE, 60*SCALE, 247*SCALE, 62*SCALE), fill=color)
    image.resize((272, 69), Image.Resampling.LANCZOS).save(
        OUT / ("header-online.png" if connected else "header-offline.png"))
