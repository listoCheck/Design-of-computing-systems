# Источники и лицензии

- CMSIS, HAL (кроме UART), startup и linker script скопированы из `lab1`;
  происхождение описано в [lab1/docs/THIRD_PARTY.md](../../lab1/docs/THIRD_PARTY.md).
- `stm32f4xx_hal_uart.c/.h` взяты из примера
  [SDK_UART производителя](https://github.com/lmtspbru/SDK-1.1M/tree/bd8cba8f4bd5fc2d1460e6ad4da7b7dd13d9e14d/SDK_UART/Drivers/STM32F4xx_HAL_Driver).
  Они не являются самостоятельно написанной частью работы; заголовки авторских прав сохранены.
- Лицензии библиотек сохранены в `Drivers/**/LICENSE.txt`, лицензия CMSIS Device —
  [CMSIS_DEVICE_LICENSE.md](CMSIS_DEVICE_LICENSE.md).
- Модули button, led и gpio_driver переиспользуют собственную реализацию лабораторной № 1.
- Самостоятельная реализация этой работы: queue, uart_driver, uart_hal (оболочка над HAL),
  uart_register (регистровый UART), console, traffic, main, сборка и тесты.
- Unicorn — внешняя тестовая зависимость, в прошивку не входит.

Технические источники: [RM0090](https://www.st.com/resource/en/reference_manual/rm0090-stm32f407-advanced-armbased-32bit-mcus-stmicroelectronics.pdf),
[PM0214](https://www.st.com/resource/en/programming_manual/DM00046982-.pdf),
[схема стенда](../../lab1/docs/SDK1.1M.pdf).
