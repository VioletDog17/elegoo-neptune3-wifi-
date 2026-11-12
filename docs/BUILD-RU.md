# Сборка и разработка

Готовые файлы для установки лежат в `firmware/v0.1.0-experimental/`; этот раздел нужен только для изменения исходников.

## Основная плата

Исходники: `neptune3pro-main/Firmware/Mainboard/Marlin-2.1.1_TJC_029/Marlin-2.1.1_TJC_029`.

Проверенные инструменты: Python 3.13, PlatformIO Core 6.1.18, platform ststm32 12.1.1, ARM GCC 9.2.1, Arduino STM32 1.9.0. Скрипт восстанавливает штатные определения плат/вариантов из включённого PlatformIO.zip и использует отдельный каталог пакетов `.platformio-wifi`.

```powershell
python -m pip install platformio==6.1.18
./wifi-tests/build.ps1 -Environment NEPTUNE3_MKS_WIFI
```

Результат: `dist/NEPTUNE3_MKS_WIFI/ZNP_ROBIN_NANO.bin` и manifest.json. Скрипт **не прошивает** принтер. Если локальная копия PlatformIO уже находится в `.tools/pio`, скрипт использует её; это необязательный каталог разработчика, в Git он не включён.

| Окружение | Назначение |
|---|---|
| NEPTUNE3_MKS_WIFI | Загрузка, запуск, статус и исходные имена; используется в опубликованном комплекте |
| NEPTUNE3_MKS_WIFI_DIAG | Только диагностика связи; загрузка/старт отключены |
| MKS_E3_V2 | Базовая сборка исходного снимка без нового Wi-Fi модуля; не дамп заводской прошивки |

Нельзя переносить BIN этих окружений на другую плату по сходству названия. Во всех случаях для SD-установки используется экспортированное имя ZNP_ROBIN_NANO.bin.

## Экран

Актуальный исходник: `neptune3pro-main/Firmware/LCD/NEPTUNE3_WIFI_NAMES.HMI`.
Модель TJC4827X243_011, портрет 272×480, UTF-8. Использован официальный [USART HMI](http://wiki.tjc1688.com/download/usart_hmi.html) 1.68.2.

Откройте HMI → Compile → File → 输出生产文件 (production output), чтобы экспортировать TFT. После изменения бинарника обновите manifest и SHA256, не оставляйте хеши старой сборки. Старые MOD/STYLE HMI сохранены для истории; не перепутайте их с актуальным NAMES.

Графические ресурсы и обзор изменений событий: `neptune3pro-main/Firmware/LCD/wifi-mod/`. Для необязательной генерации заголовков нужен Pillow и установленный Windows Arial Bold; сам TTF в репозиторий не включён.

## Тесты без принтера

Шесть C++ наборов: protocol, receiver, upload, compatibility, preview, filename_metadata. На Windows скрипт использует локальный Zig 0.13.0, если он есть в `.tools/python/ziglang`, либо Visual Studio 2022 Community C++ Build Tools по штатному пути.

```powershell
./wifi-tests/run-tests.cmd
```

Без Windows-скрипта любой набор можно собрать C++14 компилятором, например из корня репозитория:

```sh
c++ -std=c++14 -Wall -Wextra -Werror wifi-tests/filename_metadata_test.cpp -o /tmp/filename_metadata_test
/tmp/filename_metadata_test
```

Тесты проверяют кадры MKS, таймауты, ошибки/обрыв передачи, границы блоков 1024 байта, UTF-8, короткие имена, ответы слайсеру, извлечение превью и формат NAM. Полный прогон на ПК не заменяет проверку UART/SD на устройстве.

## Проверки по сети

В этих командах замените пример IP своим. Установка requests/pyserial нужна для отдельных скриптов сетевой/USB диагностики, не для прошивки готового BIN.

```powershell
python -m pip install requests pyserial
python wifi-tests/check-slicer-connection.py --host 192.168.1.50
python wifi-tests/network-upload-test.py --host 192.168.1.50
python wifi-tests/send-file.py model.gcode --host 192.168.1.50
```

Последние две команды записывают файл на карту, но сами не отправляют M23/M24. Проверяйте режим и свободное место перед запуском. Аппаратная карта интерфейсов: [HARDWARE.md](../wifi-tests/HARDWARE.md).

Исходники Wi-Fi: `Marlin/src/feature/neptune_mks_wifi/`; интеграция UART — HAL/STM32/MarlinSerial.cpp и DGUSDisplay.cpp, UI/SD — DGUSDisplayDef.cpp и cardreader.cpp. Облачные функции и произвольный G-code по сети этим модом не предоставляются.
