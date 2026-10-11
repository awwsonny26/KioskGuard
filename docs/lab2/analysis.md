# Записи та аналіз ЛР2

13.10.2026, Wokwi, STM32C031C6, форма B. HEX:
`a9bb74c53b1f2de2d63ab02fe387236d0b11d50ba049ff9e61cabe1d5d8a0fd4`.
Це цифрова модель, не вимірювання фізичного стенда.

## Відтворення

Повний captures/final-all-scenarios.vcd.gz — gzip без зміни VCD.
Шкала 1 нс, спостережений крок фронтів 25 нс. D0=SCL, D1=SDA, D2=TX,
D3=RX, D4=PIR, D5=ADC_MARK, D6=LD4, D7=PC13. Порогову напругу апаратного
аналізатора не налаштовували: його немає.

```sh
gzip -dc docs/lab2/captures/final-all-scenarios.vcd.gz > /tmp/kioskguard-full.vcd
python3 tools/vcd.py /tmp/kioskguard-full.vcd --output /tmp/kioskguard-decoded
sigrok-cli -i docs/lab2/captures/i2c-whoami.sr -P i2c:scl=SCL:sda=SDA -A i2c=addr-data
sigrok-cli -i docs/lab2/captures/uart-byte.sr -P uart:tx=TX:baudrate=115200 -A uart=tx-data
```

У PulseView відкрийте SR, додайте I²C (SCL/SDA) або UART (TX, 115200,
8N1, idle HIGH, LSB first), Zoom to Fit. SR отримано стандартним sigrok-cli
VCD input з downsample=100, skip=0: 10 МГц, крок 100 нс. VCD є джерелом
точних чисел; SR квантує фронти. Обрізані VCD зберігають SHA256 оригіналу
й offset_ns у comment. manifest.json містить хеші й межі фрагментів.
Знімки PulseView зроблено з цих SR. Обрізання прибирає рамку/покажчик/банер;
хвилі, байти та логи не ретушовано.

## I²C

WHO_AM_I: START → D0 (0x68 W) ACK → 75 ACK → STOP; окремий START → D1
(0x68 R) ACK → 68 NACK → STOP. Останній NACK формує master як штатне
завершення читання. Це не відсутність пристрою. Між pointer і read є
STOP/START, а не repeated START.

Конфігурація: 6B=01 (вихід зі sleep), 1A=02 (DLPF), 1C=08 (±4g),
1B=00 (±250°/с), 19=04 (200 Гц при DLPF). Опитування MCU — 20 мс, 50 Гц.
Burst від 3B: 00 00 00 00 20 00 EF 5C 00 00 00 00 00 00.
Big-endian signed int16: az=8192/8192=1g=1000mg;
0xEF5C=-4260, T=-4260/340+36.53=24.0006°C. Цілочисельний драйвер дає
24001 m°C. Три gyro осі нульові. Ці самі байти є host fixture.

| Параметр | Розрахунок / мета | VCD | Висновок |
|---|---|---|---|
| SCL | (63+94)×62.5нс=9.8125мкс без синхронізації | 10.000мкс, 100кГц | +1.91% до спрощеного розрахунку |
| WHO pointer | адреса й pointer | 201.500мкс | Два ACK |
| WHO read | адреса й ID | 195.000мкс | ID=68, останній NACK штатний |
| Burst read | 15×9×10мкс=1.350мс + overhead | 1.365мс | 14 байтів даних |
| Адресний fault | 0x69 W | 105.000мкс | NACK після D2, STOP |
| Fault retry | 1000мс | ~999.3–1000.0мс | Інші входи працюють |

WHO pointer START=2.863600мс, STOP=3.065100мс; read START=3.266025мс,
STOP=3.461025мс. Burst read START=6.809800мс, STOP=8.174800мс.
Fault START=16753.628700мс, STOP=16753.733700мс.
Знімки pulseview-whoami/burst/clock/nack.png. MPU Register Map Rev4.2:
§§4.2, 4.4–4.7, 4.17–4.19, 4.28, 4.32, 4.35; живлення — Specification Rev3.4.

## UART

Перший байт BOOT — B=0x42. START LOW, LSB-first 0,1,0,0,0,0,1,0,
STOP HIGH; без парності. 115200 дає 8.6806мкс/біт; BRR417 при 48МГц
дає 8.6875мкс і 115107.9біт/с (−0.080%). VCD фронти 8.675–8.700мкс
через 25нс квантування. START=8.643575мс, початок STOP=8.721775мс.
Міжсимвольний інтервал не дорівнює тривалості 10-бітового кадру.
Знімок pulseview-uart.png, sigrok незалежно декодує 42.

RX команди завершено CR. Власний decoder обробляє CR/LF без дублювання.
Логічний час t зберігається в snapshot черги; UART передає запис пізніше.

## GPIO й ADC

PIR HIGH 3833.406950..8833.406975мс = 5.000000025с, одна USER_NEAR.
До t3849 — 15.593мс; до першого TX байта — 19.655мс (<200мс).
Друга PIR HIGH у сервісі також 5с, INFO. LED змінюється ~щосекунди,
повний цикл 2с. Знімок pulseview-pir.png показує обидва фронти.

ADC_MARK охоплює весь HAL Start/Poll/Get/Stop. Медіана ширини 72.675мкс,
діапазон 70.250..84.125мкс; медіана періоду 49.9867мс. Чистий converter:
PCLK/4=12МГц, (160.5+12.5)/12МГц=14.4167мкс. Це різні величини.
Знімки pulseview-adc-period/adc-width.png.

Окремий baseline-button.vcd походить з актуального запису B1: bounce
34816.771..34819.184мс, PENDING t34853, TX34855.600мс. Від стабільного LOW
до TX ~36.4мс, узгоджується з debounce35мс і ms-квантуванням. Утримання
~24.77с не дало повторних baseline. Release/rearm у записі не перевірено;
їх покриває host тест13.

## Спільні сценарії

| Стимул | Фактичний результат |
|---|---|
| Старт ax=ay=0, az=1g, ADC1001 | PENDING t28 → READY t2049 |
| PIR + ax=2g одночасно на паузі | seq3 USER_NEAR INFO, seq4 BODY_SHOCK WARN, обидва t3849 |
| Динамічний вектор (2,0,1)g | Немає MOVED: норма поза 0.9..1.1g |
| (0.75,0,0.75)g, 45° | стабільна орієнтація t11913 → MOVED t12913 |
| service on, unknown, fault on | MODE, REJECTED, DIAGNOSTIC, одноразова SENSOR_ERROR |
| PIR і LDR при NACK | USER_NEAR INFO t19352; LIGHT_CHANGED INFO t19680; tick/SAMPLE тривають |
| fault off | RECOVERED t24757; без reconnect shock |
| LDR ~500lux, потім 0.1lux у CONTROL | LIGHT_CHANGED WARN t27030, ADC4059, старий baseline1001 |
| baseline при ax=2g | ready0 до повернення у стабільність; READY t36235, baseline4059 |

Точний 50мс спалах, межі гістерезису, service-механіка, rollover, overflow
і rearm B1 перевірено host тестами. GUI-спалаху точно 50мс або фізичного
випробування не виконували.
