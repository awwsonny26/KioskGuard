# KioskGuard

Новаківська Софія Василівна, ІО-35. Bare-metal реалізація на Nucleo-C031C6; середовище Ubuntu 24.04, Docker, STM32 HAL, Wokwi. Один super loop опитує сенсори, розбирає UART і веде аудит; RTOS немає.

Поточний код відповідає вимірювальному кандидату `candidate-spans-20261101`, ELF SHA256 `093897f8f59176f5ad30b271edb3f85f949fd2e511e66d8acfc195ac1d282cf2`. Попередні досліди стосуються іншого ELF, `candidate-poll-tree-abf27f5853950696`, і збережені окремо в [доказах](docs/metrics/lab3-observed-20261101/README.md); окремий FR-11 описаний у [перевірці](docs/metrics/lab3-fr11-20261101/README.md). Збірка не підтверджує роботу фізичної плати.

Для Wokwi збірка додатково створює `firmware-wokwi.hex`: резервовані сторінки аудиту в ньому явно ініціалізовані як стерті (`FF`). Звичайні ELF/HEX/BIN залишаються окремими. Це початковий стан стенду, а не збереження даних між запусками симулятора. Окремий дослід показав програмування Flash, але команда стирання у Wokwi повернула успіх без зміни записаних байтів; перевірка readback виявляє цю відмову. Тому повний цикл повторного використання сторінок та відновлення після втрати живлення не підтверджений.

Попередній образ дав годину S2 і 45 входів S3, але тоді аудит повідомив `failed=1, committed=0`. Новий кандидат окремо вимірює короткі ділянки TIM3 і спостережені піки стека та heap; повне завантаження CPU, реальний WCET, фізичні сенсори й power-loss залишаються непідтвердженими. Докладно: [задачі](docs/tasks.md), [вимоги](docs/PRD.md), [архітектура](docs/architecture.md), [метрики](docs/metrics/README.md), [обмеження](docs/limitations.md), [база ЛР4](docs/baseline-lab4.md).

## Збережена реалізація ЛР2

Новаківська Софія Василівна, ІО-35. ЛР2, форма B: STM32C031C6 і Wokwi.

PIR журналює рух поруч як INFO. MPU6050 розрізняє удар і стійкий нахил
корпусу, ADC фіксує стійку зміну внутрішнього світла. У контрольному режимі
механічні та світлові події мають WARN, у сервісному — INFO. Світло саме
по собі не доводить злам; PIR не визначає сеанс.

```sh
make clean
make build
make test
make coverage
make sanitize
```

Docker створює firmware/build/firmware.elf, .hex, .bin для Cortex-M0+.
App/Devices тестуються на хості без плати. CI повторює build, test, coverage
і ASan/UBSan на Ubuntu 24.04 при push та PR у main.

Відкрийте цей корінь у VS Code, F1 → Wokwi: Start Simulator.
Після зміни HEX закрийте стару вкладку симулятора й запустіть заново.
Старт: BOOT project=KioskGuard lab=2, три SELFTEST, BASELINE_PENDING;
після 2 с стабільності — BASELINE_READY. Trial для роботи не активували.
У перевіреному запуску працював Code 1.100.3 з --disable-gpu та Wokwi 4.0.
Новіший екземпляр показував порожній webview; причина не встановлена.

Команди UART, завершені CR або LF: service on, service off, baseline,
probe, fault on, fault off. Діагностичний fault звертається до відсутньої
0x69, створюючи NACK у моделі. Датчик лишається на 0x68.
Синя віртуальна кнопка B1 / PC13 відтворює активний LOW вхід B1.

- [Вимоги](docs/PRD.md), [компоненти](docs/hardware-components.md), [pinout](docs/pinout.md).
- [Архітектура](docs/architecture.md), [UART](docs/uart.md), [тести](docs/testing.md).
- [Записи та вимірювання ЛР2](docs/lab2/analysis.md), [журнал проблем](docs/lab2/problems.md).
- [Перевірки](docs/verification.md), [здача](docs/submission.md), [історія плану](docs/lab-plan.md).

Фізичну плату та бюджет живлення не випробувано. Докази форми B збережено
як VCD, SR, декодування та фрагменти справжніх знімків програм.
PR ЛР1 злитий у main; feature/lab-2 розвиває цей стан. PR ЛР2 створює користувач.

[Готовий звіт](docs/lab2-report.md).
