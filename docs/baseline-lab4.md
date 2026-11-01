# Базова лінія KioskGuard для ЛР4

> Оновлення 01.11.2026: наведений нижче опис 28.10 є історичним і стосується попередньої TIM3-версії. Актуальні експериментальні дані окремого кандидата SysTick/busy-poll і відтворюваний набір S1/S2/S3 наведено в [спостереженнях 01.11](metrics/lab3-observed-20261101/README.md). Зокрема S2 підтвердив годину прогресу, але audit повідомив про відмову; S3 містить 45 перевірених GPIO-входів під час export. Синтетичний вхід не є фізичним PIR. CPU/WCET, runtime stack/heap, power-loss і фізична Flash не перевірені. Для ЛР4 повторювати той самий набір умов, не підмінюючи відсутні значення нулями. Код RTOS тут не реалізовано.

28.10.2026. Це опис відтворення ЛР3. Код RTOS не додавався. Поточні підтверджені результати є cross-build та host-перевірками. Стендові часові таблиці поки порожні й не замінюються host-часом.

## Незмінні умови порівняння

STM32C031C6, 48 МГц, flash 32 КіБ, RAM 12 КіБ. Останні дві сторінки flash 0x08007000..0x08007fff відведені audit. USART2 115200 8N1. Сенсори й пороги ЛР2 збережені. Вимірювач TIM3 1 МГц, 16-bit CNT + overflow IRQ. Довше блокування IRQ за 65,536 мс робить це джерело неповним. Wokwi не відтворює живлення та фізичні flash delays. DWT CYCCNT не використовується на Cortex-M0+.

Версію збірки задає `tools/stamp_version.py`: base Git HEAD + хеш поточного вмісту firmware, linker, Makefile і стенда. Повний manifest та хеші ELF/HEX/BIN у `docs/metrics`. Сам HEAD без dirty tree не визначає прошивку.

```sh
make version
make build
make test
make sanitize
make coverage
docker run --rm -v "$PWD:/workspace" -w /workspace kioskguard-build:lab2 python3 tools/memory_report.py
```

## Стендові сценарії для повторення

Спочатку запустити тільки цей стенд, перевірити BOOT source, SELFTEST та CLOCK_CHECK. Підключення logger здійснюється через `rfc2217://127.0.0.1:14379`. Потрібен Python з pyserial. Logger не створює GUI-вікон і не активує trial. Якщо симулятор не запущено або порт недоступний, сценарій не виконаний.

```sh
python3 tools/serial_scenario.py S1 --out docs/metrics/S1-run-01
python3 tools/serial_scenario.py S2 --minutes 60 --out docs/metrics/S2-run-01
python3 tools/serial_scenario.py S3 --out docs/metrics/S3-run-01
python3 tools/serial_scenario.py fault --out docs/metrics/fault-run-01
python3 tools/serial_scenario.py boundary --out docs/metrics/boundary-run-01
python3 tools/trace_metrics.py docs/captures/S3-run-01.vcd --out docs/metrics/S3-vcd.json
python3 tools/check_audit_export.py docs/metrics/S3-run-01/uart.log
```

Кожен out новий. Файли попереднього запуску не перезаписуються. Новий сценарій починається з нового запуску тієї самої збірки; зберегти початковий BOOT окремо, якщо UART forwarding підключився пізніше.

| Сценарій | Умови | Мінімум даних |
|---|---|---|
| S1 | 120 с без стимулів; після baseline лише сенсори й heartbeat | Тисячі IMU/PIR/ADC запусків; STAT/HIST та короткий VCD |
| S2 | ≥3600000 мс MCU, рідкі service-команди; sensor polling незмінний | Перша/остання MCU мітка, runtime stack/heap, audit до/після, часовий ряд |
| S3 | 120 с; 40 повторів export + audit save + service; PRNG seed 35031 | Щонайменше 30 підтверджених RX→ACK, IRQ/queue loss, максимуми та VCD |
| Відмова | fault on, робота інших входів, fault off | NACK реальний у моделі, ERROR/RECOVERED, retry, лічильники |
| Межа | rate 20, 10, 5; зростання потоку service | Точне перше порушення строку/втрата; якщо не знайдено, так і записати |

Logger запускає основні команди за останньою отриманою міткою MCU. У S3 audit save та service надсилаються за окремим host-таймером через 17/33 мс після export. Інакше EXPORT міг би відкласти SAMPLE і сам стимул чекав би завершення довгої операції. Host-інтервали не є часом MCU, їх точність залежить від транспортної буферизації; read timeout у S3 становить 10 мс. Фактичне накладання перевіряти лише за RX/TX/PA8 VCD. Порядок однакових service-команд неоднозначний за втрат; такі пари не використовувати для заяви про виконання FR.

Додаткові ручні стимули: щонайменше 30 PIR HIGH у різні моменти export/flash; 2g імпульси; 45° квазістатичний поворот; світло понад 15% і короткий 50 мс імпульс; service on/off для обох категорій. Зберегти фактичні моменти, а не лише заплановані. Адресний NACK не є фізичним SDA-short. Вимкнення симуляції не доводить brownout MCU.

Старт: від reset/подачі живлення до першого читання окремо від часу після TIM3 init. Поточні AUDIT_BOOT imu_us/adc_us/pir_us вимірюють лише після ініціалізації TIM3, тому не є повним power-on latency. Повторний старт з неготовою IMU та відновленням перевірити окремо.

Живлення: зберегти останній committed generation; перервати фізичне програмування/erase на стенді з контрольованим живленням, потім mount та export. Перевірити CRC, попередній/новий snapshot та повторюваність. Host byte-cut тести є окремим доказом алгоритму. Не підмінювати ними фізичний дослід.

## Підтверджені числа до стендових запусків

| Метрика | Результат | Метод |
|---|---|---|
| Flash коду та даних | 28088 / 28672 B | GNU ld, linked map |
| Audit reserve | 4096 B | Linker + дві сторінки C031 |
| Static data+bss | 3980 B | size -A, без резервів |
| RAM з linker reserves | 8592 / 12288 B | GNU ld |
| Stack reserve | 4096 B | Linker, не виміряна глибина |
| Heap minimum reserve | 512 B | Linker, не runtime peak |
| Найбільший compiler frame | див. compiler-frames.su | LTO -fstack-usage; libc без .su |
| Запис, точки обриву | 65 | Host byte-cut model |
| Ротація erase+commit, точки обриву | 2113 | Host byte-cut model |
| Одиночні пошкоджені біти | 512 відхилено | Host CRC/commit model |
| Періодичні releases | 200003 | Host logical-clock test, не MCU |
| S1/S2/S3 timing, CPU, runtime stack/heap | Не виміряно | Потрібен окремий MCU/Wokwi run |

У ЛР4 повторити ті самі сценарії, pin mapping, UART, критерії й метод. Кожну зміну інструментарію вказати, інакше S3−S1 або порівняння з RTOS не є коректним. Гіпотези та ціна механізмів наведені в `limitations.md`.
