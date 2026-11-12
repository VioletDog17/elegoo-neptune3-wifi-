# ELEGOO Neptune 3 Pro — MKS Robin WIFI

Рабочая копия исходников ELEGOO и начальный прототип поддержки MKS Robin WIFI.

**Сейчас это не готовая прошивка для установки на принтер.** Обработчик протокола пока не подключён к UART, SD и запуску печати.

## Продолжение на другом ПК

1. Клонировать этот репозиторий.
2. Открыть папку проекта в Codex и прочитать [WIFI-STATUS.md](WIFI-STATUS.md).
3. Проект Marlin: neptune3pro-main/Firmware/Mainboard/Marlin-2.1.1_TJC_029/Marlin-2.1.1_TJC_029.
4. Новый модуль: Marlin/src/feature/neptune_mks_wifi/protocol.h внутри проекта Marlin.
5. Проверки: wifi-tests/protocol_test.cpp; run-tests.cmd использует Visual Studio 2022 Community. На другом ПК путь к средствам сборки может отличаться.
6. До назначения UART и управляющих контактов подтвердить маркировку платы, процессора и модуля. Проверить совместимость MKS в установленной версии ElegooSlicer.

Цель: ElegooSlicer → MKS Robin WIFI → файл G-code на microSD → запуск печати, с сохранением штатного экрана.

Исходный проект: https://github.com/elegooofficial/neptune3pro . Исходники получены от пользователя в виде снимка main без истории Git; точный исходный коммит неизвестен. Контрольные суммы основных файлов: wifi-tests/source-hashes.json. Лицензия исходников: GPL-3.0, см. neptune3pro-main/LICENSE.

Полная сборка пока не выполнена: загрузка инструментов STM32 завершилась сетевой ошибкой. Временные инструменты, журналы и собранные тестовые программы исключены из Git.
