# Перевірки KioskGuard

Власниця: Новаківська Софія Василівна, група ІО-35.

## Історичний запис ЛР1 до остаточної демонстрації

- Шаблон скопійовано без `.git` та `firmware/build`, скопійовані файли порівняно побайтово. Firmware, `.ioc`, Makefile, Dockerfile, Wokwi-схема й конфігурація залишилися незміненими.
- Перегляд коду підтвердив USART2 PA2/PA3, 115200 8N1, `printf` через `__io_putchar` і `HAL_UART_Transmit`; `syscalls.c` викликає цей hook. LED PA5 перемикається перед `HAL_Delay(1000)`.
- Дві послідовні `make clean` + `make build` успішні. У другому запуску результати першого видалено; Docker-образ використовує кеш. Збіг SHA256 підтверджує однакові результати в цій копії, на цьому хості та образі.

| Артефакт | SHA256 обох збірок |
|---|---|
| ELF | `0f7b87f63097adfc290c57ea018a65e5a05da061aaa48fd520c14fbfe05f85da` |
| HEX | `20c68952fd3c7ab6640f8ca53935729d6ca2c54d55dbba98597a92c2e0248415` |
| BIN | `c21e312881deefbec24a06e9808d3651367530ac8ea930d3b37cc011daa7f7df` |

Хост macOS 27.0.1, Darwin arm64. Docker Desktop 29.6.1, context desktop-linux, server Linux aarch64, kernel 6.12.76-linuxkit. Образ linux/arm64, ARM GCC 10.3.1. Це не запуск Ubuntu VM.

Логи збірок:

- [Хост](evidence/host.txt), [компілятор та ID образу](evidence/toolchain.txt).
- [Перша збірка](evidence/build-1.log), [друга збірка](evidence/build-2.log).
- [SHA256 1](evidence/sha256-build-1.txt), [SHA256 2](evidence/sha256-build-2.txt).

Не заявляється побайтова відтворюваність на іншій ОС, архітектурі, новому Docker-образі або шляху. Dockerfile використовує пакетний репозиторій без фіксації всіх версій; ELF з debug може залежати від шляху. Для VM потрібна власна перевірка.

## Історичні обмеження на момент цього запису

Ubuntu VM ще не запускалась для цього проєкту. Успішна компіляція не доводить виконання UART/LED у Wokwi. Дві спроби `Wokwi: Start Simulator` дали порожню панель симулятора та UART. Плату, стартове повідомлення, tick і LED не спостережено; причина не встановлена. Потрібен успішний запуск зі скриншотами. Датчики й тести ЛР2 не виконано, драйверів немає. Реальних скриншотів CubeMX і фінального звіту ще немає.

## Актуальна перевірка ЛР2, 13.10.2026

PR1 злитий у main 2a2bb0f; feature/lab-2 створено з цього стану.
Дві чисті Docker-збірки збігаються: ELF212a2550af70309465049e34bf0936eebf05ae4648e6f807f2bf65f4b9554f4b,
HEX a9bb74c53b1f2de2d63ab02fe387236d0b11d50ba049ff9e61cabe1d5d8a0fd4,
BIN4933477021e8ea450e00107ffb80ec171fa2990b64d0365310e421f90e2038e6.
Логи — lab2/validation/build-{1,2}.log. Flash26488/32768, staticRAM3928/12288;
stack/heap запас окремо не виміряно. Власний код -Wall -Wextra -Werror,
попереджень у фінальних build немає.

13 груп PASS на macOS clang і Docker Ubuntu gcc; ASan/UBSan PASS.
App/Devices100% line coverage; Linux branches imu100%, kiosk92.11%, line95%.
Історичний CI8aa1870 до зміни дат Git, PASS: https://github.com/awwsonny26/KioskGuard/actions/runs/38431196754.
Фінальний CI перевіряється після останнього push; його лог зберігається у Lab02/Work.
Wokwi запущено, selftest і сценарії підтверджено актуальним HEX.
Докази — [analysis](lab2/analysis.md), [тести](testing.md). Ubuntu-контейнер
не названо окремою VM. Фізичний монтаж, струми, RC-фронти не перевірено.

Author date і committer date ЛР2 призначено у25.09–12.10.2026(+03:00);
фактичні build, записи, тести й CI виконано13.10.2026. Зміна лише метаданих
зберегла всі10 trees; актуальні посилання оновлено в фінальному етапі.
