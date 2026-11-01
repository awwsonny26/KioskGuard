# KioskGuard: підсумок вимірювань ЛР3

STM32C031C6, один super loop, SysTick 1 мс та активне опитування. Ідентичність ELF, метод і первинні дані: [S1/S2/S3](metrics/lab3-observed-20261101/README.md), [FR-11](metrics/lab3-fr11-20261101/README.md). S2 дав годину безперервного прогресу MCU; S3 — 45 синтетичних PIR-входів під час export. Один окремий одночасний синтетичний GPIO/I²C стимул дав `BODY_SHOCK WARN` і `USER_NEAR INFO` до 200 мс.

AUDIT повідомив failed=1, committed=0: постійне збереження не підтверджене. Немає фізичних випробувань, CPU/WCET і runtime піків stack/heap. Host-модель CRC/commit не є фізичним power-loss тестом. Детальні [задачі](tasks.md), [обмеження](limitations.md) і [база ЛР4](baseline-lab4.md).
