# Лабораторная работа 3 — OLED-дисплей и устройства I²C

Вариант 1 «Часы» для SDK-1.1M / STM32F427. Прошивка читает и устанавливает
внешние часы реального времени MCP79411 по I²C1, непрерывно выводит время,
дату, день недели и состояние на OLED SSD1306, а также обменивается данными с
графическим клиентом по USART6 (57600, 8N1).

## Состав

- `Core/Src/rtc_device.c` — MCP79411: BCD, флаги ST/OSCRUN/PWRFAIL, безопасная
  асинхронная установка времени;
- `Core/Src/oled.c`, `gfx.c` — SSD1306 128×64 и независимые графические
  примитивы; данные экрана отправляются по одной странице;
- `Core/Src/bus_hal.c` — единственный владелец I²C1 (PB8/PB9, 400 кГц), режим
  опроса;
- `Core/Src/scheduler.c` — TIM6 раз в 250 мс запускает обновление, но I²C в
  прерывании не используется;
- `client/app.py` — GUI на Tkinter. Аргумент запуска — имя COM-порта. Режим
  `DEMO` намеренно имитирует стенд и помечает строки журнала как `demo: true`.

## Сборка и проверки

```powershell
python lab3/tools/build.py
python lab3/tests/run_tests.py
python -m unittest discover -s lab3/tests -p test_client.py -v
```

Первые тесты компилируют и исполняют настоящий ARM-код в Unicorn; плата не
нужна. Они не являются электрическим тестом I²C/OLED. Подробные результаты и
границы проверки — в [документе валидации](docs/VALIDATION.md).

## Запуск клиента

```powershell
python -m pip install --target lab3/build/python-deps -r lab3/client/requirements.txt
python lab3/client/app.py COM5
# без стенда — только демонстрация интерфейса:
python lab3/client/app.py DEMO
```

После открытия порта первым кадром обязательно посылается `PING`; элементы
установки времени доступны лишь после `PONG`. Лог UART в JSONL по умолчанию
создаётся в `lab3/logs/`; путь можно задать ключом `--log`.

Описание протокола — [PROTOCOL.md](docs/PROTOCOL.md), оформление работы и
руководство пользователя — [REPORT.md](docs/REPORT.md).
