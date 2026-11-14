# Neptune 3 Pro Wi-Fi Control · v0.2.0-dev

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
