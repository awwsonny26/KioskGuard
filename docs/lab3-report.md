# KioskGuard: підсумок вимірювань ЛР3

STM32C031C6, один super loop, SysTick 1 мс та активне опитування. Ідентичність ELF, метод і первинні дані: [S1/S2/S3](metrics/lab3-observed-20261101/README.md), [FR-11](metrics/lab3-fr11-20261101/README.md). S2 дав годину безперервного прогресу MCU; S3 — 45 синтетичних PIR-входів під час export. Один окремий одночасний синтетичний GPIO/I²C стимул дав `BODY_SHOCK WARN` і `USER_NEAR INFO` до 200 мс.

У попередньому образі AUDIT повідомив failed=1, committed=0. [Новий вимірювальний ELF](metrics/lab3-spans-20261101/README.md) дав спостережені піки stack/heap та 45 S3-входів; при ініціалізації FF записав 64 CRC-valid покоління, після чого стирання не пройшло readback. Постійне збереження, фізичні випробування та повний CPU/WCET не підтверджені. Host-модель CRC/commit не є фізичним power-loss тестом. Детальні [задачі](tasks.md), [обмеження](limitations.md) і [база ЛР4](baseline-lab4.md).
