# Журнал проблем ЛР2

| Симптом | Причина / певність | Виправлення, коміт |
|---|---|---|
| ADC бракувало в ARM build | ЛР1 шаблон не містив ADC/ADCEx/LL_ADC | Незмінені ST v1.0.1 файли й Makefile; 3c7ce10 |
| MPU мав ACK адреси й pointer, але не повертав ID | VCD показав зупинку HAL Mem_Read після pointer у моделі | Platform adapter: окремі STOP/START, очищення CR2/STOP, 5мс timeout, pacing TXDR; 9769e85, db44200 |
| gcov спочатку не давав підсумку | gcno мали префікс test_kiosk, source paths залежали від cwd | Фактичні gcno й окремі platform directories; 2b8adb4 |
| Порожній Wokwi webview | Причина невідома; reload не усунув | Code1.100.3 --disable-gpu, власний профіль Wokwi, без trial; коміт 9 (8a01998) |
| Native typeText не передавав команд | RX у VCD був порожній; причина маршрутизації невідома | Paste з CR дав RX і відповіді; decoder CR/LF; коміт 9 (8a01998) |
| Графічна B1 не створювала фронтів | У власному записі D7 не змінювався | PC13 input pull-up + віртуальний контакт GND; bounce/debounce/hold перевірено; коміт 9 (8a01998) |
| PA2/PA3 помилково названо D1/D0 | Змішані USART2/VCP й Arduino USART1 | UM2953: PA2=CN7.37, PA3=CN10.10; D1/D0=PB6/PB7; коміт 9 (8a01998) |

Хеш коміту9 наведено у звіті після фіксації. Інші коміти відкриваються за
https://github.com/awwsonny26/KioskGuard/commit/ + хеш.
GUI проблеми не названо дефектами прошивки. Не приписуємо тестам виправлень,
яких вони не знаходили. Історичний HAL failure VCD лишається в Lab02/Work.

## Внесок за шістьма напрямами

Індивідуальна робота: за кожен напрям відповідає Новаківська Софія Василівна, ІО-35.

| Напрям | Артефакти |
|---|---|
| 1. Звіт і документація | PDF, журнал |
| 2. Документація й pinout | pinout.md, ioc, diagram.json, RM/DS/UM |
| 3. Стенд форми B | Wokwi, VCD/SR, справжні знімки; фізичних вимірювань немає |
| 4. Декодування | analysis.md, tools/vcd.py, sigrok, PulseView |
| 5. Структура і build | Core/Platform/Devices/App, Docker, Makefile, dependency check, CI |
| 6. Тести | 13 груп, captured fixture, coverage, ASan/UBSan |
