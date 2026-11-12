# v0.1.0-experimental — WIFI NAMES

[Скачать комплект ZIP](Neptune3-WiFi-v0.1.0-experimental.zip?raw=true) · [Установка](INSTALL-RU.md) · [Состав и SHA256](manifest.json) · [SHA256SUMS](SHA256SUMS.txt)

| Файл | Назначение | Цель сборки |
|---|---|---|
| [ZNP_ROBIN_NANO.bin](mainboard/ZNP_ROBIN_NANO.bin?raw=true) | Основная плата | Neptune 3 Pro, ZNP Robin Nano_DW V2.2, marlin_MKS_F401RC, offset 0x8000 |
| [NEPTUNE3_WIFI_NAMES.tft](screen/NEPTUNE3_WIFI_NAMES.tft?raw=true) | Дисплей | TJC4827X243_011, 272×480, UTF-8 |

Модуль: MKS Robin WIFI V1.1 со штатным протоколом MKS. Механика/конфигурация — штатный Neptune 3 Pro. Для Plus/Max и других плат/экранов файлы не предназначены. Нужны оба файла, они обновляются через разные слоты SD.

Последняя реализация постоянных имён не проверена на принтере; сборка, шесть наборов тестов и симулятор экрана пройдены. Проверки предыдущих сборок перечислены в основном README. BIN — приложение без загрузчика, не резервный дамп. Внутреннее поле версии UI может показывать 1.4; мод создан из исходного HMI проекта ELEGOO.

Исходники находятся в этом репозитории, `neptune3pro-main/`; актуальный HMI — `Firmware/LCD/NEPTUNE3_WIFI_NAMES.HMI`. Параметры и хеши записаны в manifest.json, версия среды mainboard — NEPTUNE3_MKS_WIFI. ZIP содержит BIN/TFT, README, установку, manifest и SHA256.
