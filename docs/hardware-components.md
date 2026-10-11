# Компоненти KioskGuard, стан ЛР2

STM32C031C6: Cortex-M0+, Flash32КіБ, SRAM12КіБ. Три одночасні входи:
PIR GPIO PA1, MPU6050 I²C PB8/PB9, LDR ADC PA0. USART2 PA2/PA3, LED PA5.
Схема, AF, Arduino/Morpho, живлення й альтернативи — [pinout](pinout.md).

Повний Register Map MPU-6000/6050 Rev4.2 (46 сторінок) отримано й прочитано
до реалізації драйвера: адреса й WHO_AM_I, порядок init, signed big-endian,
scale8192LSB/g при ±4g, temperature raw/340+36.53. Product Specification
Rev3.4 (52 сторінки) містить живлення, струми та I²C timing. Локальні оригінали
— Lab02/Work/datasheets; register map у manufacturer PDF з дзеркала, specification
— [manufacturer PDF у CDI](https://www.cdiweb.com/datasheets/invensense/mpu-6050_datasheet_v3%204.pdf).
Оригінальні TDK URL раніше повертали404/redirect; ця проблема не є дефектом firmware.

ST DS13867 визначає GPIO/AF/межі струму, RM0490 — RCC/I²C/ADC/UART,
UM2953 — бортові роз'єми й VCP перемички. Локальна UM — Rev1; актуальна
[ST UM2953 Rev2](https://www.st.com/resource/en/user_manual/um2953-stm32-nucleo64-board-mb1717-stmicroelectronics.pdf)
звірена за Figure7 і Table11. Table12 призначень ніжок корпусу не є pinout роз'ємів.

Фізичні моделі PIR/LDR не обрано. [PIR Wokwi](https://docs.wokwi.com/parts/wokwi-pir-motion-sensor)
використано з 5-секундним HIGH, [LDR Wokwi](https://docs.wokwi.com/parts/wokwi-photoresistor-sensor)
живиться від3V3 й має AO0..3V3. Характеристики моделей не є datasheet довільного
реального модуля. Додаткова віртуальна B1 замикає PC13 на GND.

Проєктний бюджет3V3: breakout MPU10мА, LDR5мА, дві4.7кОм підтяжки1.4мА,
разом16.4мА. Це резерви оцінки, а не виміряні максимуми. Регулятор UM2953
§6.5.2 має500мА сумарно з платою. Споживання плати й гарантований запас невідомі.
PIR OUT потрібно перевірити на3.3В перед фізичним монтажем.

SPI flash, SCCB camera, servo не входять у погоджений PRD. Тому їхні
транзакції, живлення або unit-тести не приписуються цьому проєкту.
