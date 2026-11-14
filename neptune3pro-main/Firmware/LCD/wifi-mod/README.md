# WIFI MOD screen source

Development revision: `../NEPTUNE3_WIFI_CONTROL.HMI`, exported as `NEPTUNE3_WIFI_CONTROL.tft`. Restores the stock localized Files title and draws a small refresh arrow within the existing title touch target (VP 0x2203 value 1). Uses `printpause.t0.font=0` before page initialization for Cyrillic names. See `docs/SCREENS-RU.md` for simulator captures and `docs/WEB-CONTROL-RU.md` for the matching three-part development update.

Latest functional revision: `../NEPTUNE3_WIFI_NAMES.HMI` retains STYLE and changes only `printpause.t0.txt_maxl` from 36 to 128. Export to `dist/NEPTUNE3_WIFI_NAMES/screen/`. Requires matching mainboard filename metadata support; see `WIFI-NAMES-UPDATE-RU.md` at repository root.

Latest cosmetic revision: `../NEPTUNE3_WIFI_MOD_STYLE.HMI`, exported to
`dist/NEPTUNE3_WIFI_STYLE/NEPTUNE3_WIFI_MOD_STYLE.tft` (screen update only).
`style-events.json` records its changed handlers. Resources 192/193 are
`assets/header-offline.png` and `assets/header-online.png`; regenerate them with
`generate-header.py`. The original title is raster artwork with no retained font
metadata. The revision uses Arial Bold 18 px to match its bold visual style and
fit the longer title. About uses the existing font 0, transparent background and
the existing table alignment. The Wi-Fi indicator is black/white with no text.

Editable project: `../NEPTUNE3_WIFI_MOD.HMI`; original: `../3D30_20221121.HMI`.
Target retained: TJC4827X243_011, portrait 272x480, UTF-8, existing fonts/resources.
Built with official USART HMI 1.68.2, portable copy in `.tools/tjc-portable/USART HMI` on the development PC.

`events.json` records the changed event code and global declaration for review.
All other page/component event handlers were compared with the original and retained.
Refresh is on the first file page only: header t25 emits VP0x2203/data1.
Mainboard sets global `wifi_state`: 0 unknown/stale, 1 disconnected, 2 connected.
The main timer draws the title and Wi-Fi symbol every 400ms. About adds the author credit after page initialization.

Open HMI in USART HMI, Compile, then File → 输出生产文件 (production output) to export TFT.
Use the actual target model, preserve UTF-8 and the existing font library. Copy the exported TFT into the bundle screen folder and regenerate the bundle manifest hashes after changes.
Installation and hardware verification: `../../../../WIFI-MOD-UPDATE-RU.md` from the repository root.
This is a source build from the repository's HMI, not a readback of the installed UI 1.4.1.
