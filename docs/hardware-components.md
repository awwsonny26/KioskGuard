# Компоненти й інтерфейси KioskGuard

Власниця: Новаківська Софія Василівна, група ІО-35.

Датчики описані для ЛР2, у поточному `diagram.json` є лише Nucleo-C031C6. У ЛР1 прошивку й конфігурацію не змінено.

## Платформа та поточні з'єднання

STM32C031C6 має Cortex-M0+, до 32 КБ Flash і 12 КБ SRAM, ADC та I²C/USART. Джерело: [ST DS13867 Rev 4, с. 1](https://www.st.com/resource/en/datasheet/stm32c031c6.pdf#page=1). Не застосовувати параметри Cortex-M4/F4 з прикладу методички.

| Функція | Перевірений стан шаблону | Доказ |
|---|---|---|
| UART | USART2 TX=PA2, RX=PA3, 115200, 8 data bits, 1 stop bit, parity none | `main.c`, `stm32c0xx_hal_msp.c`, `.ioc`; TX монітора під'єднаний до PA3, RX до PA2 |
| LED | LD4 на PA5, високий рівень вмикає LED | [Wokwi Onboard LED](https://docs.wokwi.com/parts/board-st-nucleo-c031c6#onboard-led), `main.h`; `HAL_GPIO_TogglePin`, `HAL_Delay(1000)` |
| I²C1 | У шаблоні PB8 SCL, PB9 SDA; сенсора немає | `.ioc`, `stm32c0xx_hal_msp.c`; Timing=0x20303E5D, фактичну частоту ще не виміряно |
| ADC/PIR | Підключення і код відсутні | Поточні `.ioc`, схема та `main.c` |

[Wokwi Simulation features](https://docs.wokwi.com/parts/board-st-nucleo-c031c6#simulation-features) підтверджує підтримку GPIO, USART, I²C master та ADC. RCC підтримується частково; це не фізична перевірка електроживлення, шумів чи точності датчиків.

## Запланований набір

| Компонент | Інтерфейс і сигнали | Завдання | Підтвердження й межі |
|---|---|---|---|
| `wokwi-pir-motion-sensor` | GPIO, OUT; VCC/GND | Рух користувача поруч | [Wokwi Pin names/Attributes](https://docs.wokwi.com/parts/wokwi-pir-motion-sensor). OUT цифровий. Типові параметри моделі delayTime=5 с, inhibitTime=1,2 с. Це не датчик сеансу чи гарантованої присутності |
| `wokwi-mpu6050` | I²C, SDA/SCL; AD0; VCC/GND | Механічні відхилення корпусу | [Wokwi MPU6050](https://docs.wokwi.com/parts/wokwi-mpu6050). Адреса 0x68, при AD0 HIGH 0x69; прискорення в g, обертання в °/с. XDA/XCL не реалізовані моделлю |
| `wokwi-photoresistor-sensor` | ADC через AO; VCC/GND; DO не використовувати | Зміна внутрішнього світла | [Wokwi Operation](https://docs.wokwi.com/parts/wokwi-photoresistor-sensor#operation). AO є серединою дільника з LDR та 10 кОм; напруга змінюється зі світлом. Не заявляти точний люксметр |

GPIO, I²C та ADC утворюють три різні інтерфейси сенсорів. UART служить журналу та сервісним командам, LED є heartbeat.

## Дослідження MPU6050

Документи виробника: [PS-MPU-6000A-00 Rev 3.4](https://invensense.tdk.com/wp-content/uploads/2015/02/MPU-6000-Datasheet.pdf), [RM-MPU-6000A-00 Rev 4.2](https://invensense.tdk.com/wp-content/uploads/2015/02/MPU-6000-Register-Map1.pdf). Посилання на PS повернуло 404, на RM перенаправило на пошук TDK. Наведені нижче сторінки й значення перевірені за індексованими витягами саме офіційних PDF. Для фінального звіту потрібні повні PDF.

| Що | Підтверджені дані | Сторінка |
|---|---|---|
| Живлення чипа | VDD 2,375..3,46 В; VLOGIC не більше VDD | PS, с. 14, Electrical and Other Common Specifications |
| Струм чипа | Типовий 3,8 мА для gyro+accel, DMP disabled; 3,9 мА з DMP | PS, с. 14; не споживання всієї плати GY-521 |
| Шина | I²C до 400 кГц, 7-бітні адреси 0x68/0x69 залежно від AD0; потрібні pull-up SDA/SCL | PS, с. 33, Digital Interface |
| Ідентифікація | WHO_AM_I 0x75, типове значення 0x68; AD0 не відображається в цьому регістрі | RM, с. 45, Register 117 |
| Вихід зі sleep | PWR_MGMT_1 0x6B, SLEEP bit 6, вибір clock у CLKSEL | RM, с. 40, Register 107 |

Перед драйвером ЛР2 додатково звірити ACCEL_CONFIG, GYRO_CONFIG, CONFIG, SMPLRT_DIV та регістри вимірювань із повним RM. Обрати діапазон без насичення на тесті 2 g. Не копіювати Arduino-бібліотеку з прикладу Wokwi. В STM32 HAL 7-бітну I²C-адресу передавати у форматі, якого вимагає HAL API, після перевірки локального заголовка та реалізації.

## Живлення, піни та невідомі характеристики

Для моделі планується спільна GND і 3V3, MPU6050 AD0 на GND, SDA/SCL на вже налаштовані PB9/PB8. Розводку PIR OUT і LDR AO затвердити в ЛР2 після перевірки таблиці pin/alternate functions STM32 та відсутності конфліктів. До цього піни не змінювати.

AO підключати лише в межах допустимого аналогового входу та живлення ADC. Не подавати AO 5 В лише тому, що цифровий GPIO може мати 5V tolerance. Таблиця напруг Wokwi для LDR наведена при VCC=5 В і прикладом Arduino ADC; її числа не є калібруванням STM32. Планується 12-бітне вимірювання raw 0..4095 та відносна зміна, без переведення в люкси до калібрування.

PIR і LDR у Wokwi не мають обраної фізичної моделі виробника. Тому робоча напруга, струм, чутливість та офіційні datasheet реальних PIR/LDR залишаються непідтвердженими. Не приписувати їм характеристики HC-SR501 або GM5528 без вибору цих модулів. Перед фізичною реалізацією потрібно вибрати виробника/модель, знайти його PDF, сторінки з VCC/OUT/струмом і схемою дільника; перевірити бюджет 3V3 та рівні сигналів. Регістрової карти в простого аналогового LDR і цифрового PIR немає.

## Джерела

- ST DS13867 Rev 4, с. 1, 20, розділ 4 для MCU, ADC і pin-mapping.
- TDK PS-MPU-6000A-00 Rev 3.4, с. 14 і 33; RM-MPU-6000A-00 Rev 4.2, с. 40 і 45. Доступ описаний вище.
- Офіційні сторінки Wokwi у таблицях.
- Шаблон `SP-Boilerplate`, [вихідний приклад HAL](https://github.com/wokwi/stm32-hello-wokwi). `LICENSE` і ліцензії ST/CMSIS збережено.
- Локальна `Materials/SP/LR1.pdf`, с. 3, 5..9; уточнення викладача 23.09 у `Materials/SP/TOPICS.md`.
