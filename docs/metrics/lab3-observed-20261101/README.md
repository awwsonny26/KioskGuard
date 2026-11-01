# ЛР3: зафіксовані результати KioskGuard

Виміряна окрема діагностична збірка `candidate-poll-tree-abf27f5853950696`, ELF SHA256 `58e5ab7fd10281b0df4a39b6f53b9b3df29d12f2352092f8791329d26027c54c`. Архів `frozen-source.tar.gz` містить її джерела firmware, Dockerfile і Makefile; точні ELF/HEX та схеми лежать у `tools/lab3-s3-fixture/workspace`. Основне дерево firmware узгоджено з цим образом; ці цифри не слід приписувати іншій збірці.

| Дослід | Спостереження | Межа |
|---|---|---|
| S1 | 120000 мс MCU, 241 SAMPLE, 240 інтервалів по 500 мс | без команд; немає повної runtime-статистики |
| S2 | 3600000 мс MCU; для розширених меж 384500–3985000 VCD показує 3600425,035 мс | у всіх 13 AUDIT: failed=1, committed=0; це негативний результат збереження |
| Gate | 3 завершені export-вікна з незалежними D4/D7, INFO USER_NEAR та командами RX | синтетичний GPIO замінює OUT моделі PIR |
| S3 | 45 із 45 вікон із таким самим накладанням; D4→D7 1,889–29,713 мс; D4→кінець TX 15,883–61,378 мс | лише дана симуляція і навантаження |
| I²C NACK | ERROR після RX 26,219 мс; RECOVERED після fault off 996,217 мс | зміна адреси в ПЗ, не фізичний обрив |
| Період IMU | середнє при rate 20/10/5: 19999,458 / 9999,674 / 4999,884 мкс | обмежений перебір, точку зламу не встановлено |

У S3 для 281 service-команди декодовано 281 відповідний завершений TX; максимум 40,002 мс, RXDIAG errors/drop/armfail=0. Див. `vcd-review.json`, `suite-observations.json`, `comparison.json` і raw. S1/S3: максимальний інтервал IMU 20,213/31,767 мс, 0/13 інтервалів понад 30 мс; це не лічильник пропущених release. Для точного відтворення архіви VCD розпаковуються без змін; їх SHA256 після розпакування наведені в JSON. `s1-and-interrupted.vcd.gz` містить S1 і старий перерваний S2; старий інтервал не складався з новим.

CPU/WCET, runtime stack/heap, фізична надійність Flash та power-loss не виміряні. Вимоги FR-14 і повну номінальну роботу не підтверджено; FR-11 у S1/S2/S3 не досліджено; окремий синтетичний дослід наведений у [FR-11](../lab3-fr11-20261101/README.md). Фізичний PIR та реальне від'єднання I²C не досліджено. Результати є частковою, відтворюваною базою для ЛР4, не виконанням ЛР4. Середовище: Ubuntu 24.04, Docker, STM32 HAL, Wokwi.

## Перевірена збірка із зафіксованого архіву

Після checkout гілки `feature/lab-3` наступні команди розпаковують тільки зафіксований архів у нову тимчасову директорію, створюють окремий Docker image та збирають код, не змінюючи робоче дерево:

```sh
kg_tmp=$(mktemp -d)
tar -xzf docs/metrics/lab3-observed-20261101/frozen-source.tar.gz -C "$kg_tmp"
docker build -t kioskguard-provenance:20261101 -f "$kg_tmp/docker/Dockerfile" "$kg_tmp"
docker run --rm -v "$kg_tmp:/workspace" -w /workspace kioskguard-provenance:20261101 make -C firmware BUILD_DIR=build-poll
shasum -a 256 "$kg_tmp/firmware/build-poll/firmware.elf" "$kg_tmp/firmware/build-poll/firmware.hex"
```

1 листопада 2026 р. ця послідовність дала ELF `58e5ab7fd10281b0df4a39b6f53b9b3df29d12f2352092f8791329d26027c54c` і HEX `0445490790f09b7cb904ce04b37c37792026bbe60bd6f51f00e90963cd4ef513`; відповідні контрольні образи знаходяться у `tools/lab3-s3-fixture/workspace/firmware/build-poll/`. Dockerfile фіксує версії залежностей; їх майбутня доступність у репозиторіях пакунків не гарантована. Ця перевірка є ARM-збіркою, не новим запуском Wokwi або фізичного MCU.
