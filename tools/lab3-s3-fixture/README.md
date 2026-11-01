# Стенд S3 для KioskGuard

Це архів стенду, який 1 листопада 2026 р. використовував заморожений ELF `58e5ab7f…26027c54c` та HEX `04454907…cd4ef513`. Готові результати й захвати — у `../../docs/metrics/lab3-observed-20261101/`. Вихідні значення і хеші описані там. Основне робоче дерево firmware може відрізнятися від експериментального образу; джерело кандидата збережене в `frozen-source.tar.gz`.

`workspace` і `workspace-suite` — окремі Wokwi-проєкти з незмінним образом. Перед відтворенням створіть у кожному `docs/captures`, перевірте що порт 14379 вільний і що вихідні VCD та каталоги результатів відсутні. Python-збирачі потребують pyserial 3.5. Не замінюйте наявні VCD.

Вихід штатної моделі PIR від'єднано від PA1: `pirfixture` пасивно читає TX і генерує на тому самому вході високий рівень 5 с, потім щонайменше 1,2 с низького рівня. Це тест GPIO-інтерфейсу, не фізична модель датчика. Зерно 35031 задає фази після справжнього `EXPORT slot=0`; не створюються підроблені UART-відповіді MCU.

1. Запустіть Wokwi із `workspace`, після старту порту — `python3 run_suite.py gate`. Зупиніть модель і збережіть `workspace/docs/captures/gate.vcd`.
2. Запустіть `python3 verify_vcd.py gate workspace/docs/captures/gate.vcd`. Не продовжуйте, якщо три незалежні підтверджені вікна не пройшли.
3. Запустіть окремий проєкт `workspace-suite`, потім `python3 run_suite.py suite`. Після Stop збережіть `workspace-suite/docs/captures/suite.vcd`.
4. Виконайте `python3 verify_vcd.py suite workspace-suite/docs/captures/suite.vcd` та `python3 summarize_suite.py`; збережіть raw і JSON. Лише фактичні пари з D4, D7, INFO та RX у завершеному export-вікні зараховуються.

У suite `fault on/off` змінює I²C-адресу, а `rate 20/10/5/20` досліджує обмежений набір навантажень. CPU/WCET і runtime stack/heap цей кандидат не експортує. Подія й UART-рядок не доводять фізичне відновлення Flash чи втрату живлення. Хостові mock-тести не є симуляційними доказами.

API та формат: https://docs.wokwi.com/guides/custom-chips-to-wasm .
