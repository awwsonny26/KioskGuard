# Передача KioskGuard у Ubuntu VM

Власниця: Новаківська Софія Василівна, група ІО-35.

Основна копія на Mac: `Study/People/Новаківська Софія Василівна/2026-Fall/SP/Lab01/Work/KioskGuard`. Архів `KioskGuard-VM-2026-10-03.tar.gz` та його `.sha256` лежать поруч у Work, поза репозиторієм. Він містить вихідні файли, документи й Mac-логи, без `.git`, build та сценарію відео. Git-історію демонструють з основної копії; цей архів не є Git-репозиторієм.

## Передача архіву

У командах замініть `pliffdax@ubuntu-vm` на користувача й адресу своєї VM.

На Mac у Work перевірити архів:

```sh
cd '/Users/pliffdax/Projects/university/Study/People/Новаківська Софія Василівна/2026-Fall/SP/Lab01/Work'
shasum -a 256 -c KioskGuard-VM-2026-10-03.tar.gz.sha256
tar -tzf KioskGuard-VM-2026-10-03.tar.gz
```

У нову папку передачі, без перезапису старих робіт:

```sh
ssh pliffdax@ubuntu-vm 'mkdir -p /home/pliffdax/study/2026-autumn/People/NovakivskaSofia/SP/Lab01/Incoming'
rsync -avn KioskGuard-VM-2026-10-03.tar.gz KioskGuard-VM-2026-10-03.tar.gz.sha256 pliffdax@ubuntu-vm:/home/pliffdax/study/2026-autumn/People/NovakivskaSofia/SP/Lab01/Incoming/
```

Переглянути dry-run. Якщо файли призначені саме цій роботі, повторити з `-av` замість `-avn`; не додавати `--delete`.

В Ubuntu terminal перевірити новизну папки та розпакувати:

```sh
cd /home/pliffdax/study/2026-autumn/People/NovakivskaSofia/SP/Lab01/Incoming
sha256sum -c KioskGuard-VM-2026-10-03.tar.gz.sha256
tar -tzf KioskGuard-VM-2026-10-03.tar.gz
test ! -e ../KioskGuard && tar -xzf KioskGuard-VM-2026-10-03.tar.gz -C ..
cd ../KioskGuard
```

Якщо KioskGuard вже існує, зупинити розпакування й порівняти вміст, обрати нову папку. Не видаляти існуючу роботу.

## Збірка в VM

Середовище: Ubuntu 24.04, Docker, STM32 HAL, Wokwi. Docker повинен працювати без sudo.

```sh
cat /etc/os-release
uname -sm
docker --version
docker info
mkdir -p ../VM-evidence
set -o pipefail
{ date -Is; make clean; make build; } 2>&1 | tee ../VM-evidence/build-1.log
sha256sum firmware/build/firmware.elf firmware/build/firmware.hex firmware/build/firmware.bin > ../VM-evidence/sha256-1.txt
{ date -Is; make clean; make build; } 2>&1 | tee ../VM-evidence/build-2.log
sha256sum firmware/build/firmware.elf firmware/build/firmware.hex firmware/build/firmware.bin > ../VM-evidence/sha256-2.txt
diff -u ../VM-evidence/sha256-1.txt ../VM-evidence/sha256-2.txt
```

Перевірити exit code обох `make build` і зупинитися за помилки. Порожній diff підтверджує збіг тільки цих двох VM-збірок. Зберегти ОС, архітектуру, ID образу, версію компілятора й дату запуску. Не вимагати тотожності ELF з Mac до аналізу різниці шляху/тулчейну.

Wokwi можна запускати у VS Code в VM або на Mac. Якщо на Mac, спочатку перенести VM-артефакти в окрему вхідну копію проєкту й перевірити, що `wokwi.toml` вказує саме на них. Зберегти вихідні Mac-артефакти та їх SHA256. У VS Code відкрити обрану копію, F1 → `Wokwi: Start Simulator`, побачити старт, tick і LED. Ліцензія вже є, trial не активувати.

## Повернення доказів

Не редагувати одночасно основну й VM-копії. Логи й зображення спершу передати в нову `Lab01/Work/VM-Inbox-KioskGuard`, виконати `rsync -avn`, тоді `-av`. Порівняти з основною копією, лише після цього додати потрібні файли. Звіт створювати в сусідній Reports. Приклад і правило збереження основної копії наведено в `Study/VM-transfer.md`.
