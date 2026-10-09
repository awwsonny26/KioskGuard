# Перевірки переносимого коду

Дата: 13.10.2026. 13 груп assert-тестів: ID і порядок конфігурації MPU,
NACK/timeout кожної транзакції, неправильний ID, signed conversion, raw frame
з власного VCD, стабільність baseline, PIR 5 с, shock/cooldown, 45°/180°,
переривання orientation, світло/гістерезис/50 мс, режими/команди,
simultaneous PIR/shock, fault/recovery, tick wrap, queue overflow, CRLF і debounce.

Команди: make test, make sanitize, make coverage.
На macOS coverage запускається як make coverage GCOV='xcrun llvm-cov gcov'.
Мета ≥90% рядків і ≥80% напрямків гілок для App і Devices обрана, щоб перевірити
штатні переходи, граничні умови та відмови. Core, Platform і vendor не входять
до host coverage. Їхні інтеграція та сигнали перевіряються ARM-збіркою й Wokwi.

macOS arm64 clang: усі 13 груп PASS, ASan/UBSan без діагностик.
Line coverage imu.c 100%/27, kiosk.c 100%/176, line.c 100%/25.
Branch taken: imu.c 100%/14, kiosk.c 91.82%/110, line.c 95%/20.
Повні Linux і CI результати записуються після фактичного виконання у validation.

Одна fixture походить із власного запису KioskGuard, а не із MuseGuard:
00 00 00 00 20 00 EF 5C 00 00 00 00 00 00 при ±4 g.
Вона дає AZ=1000 mg і T=24001 m°C, як у UART. Інші stimuli синтетичні,
що дозволяє перевірити точні межі 299/300, 999/1000, 499/500 мс.
Fake bus повертає регістри та статуси, assertions перевіряють порядок записів
як mock expectations. assert не дає isolation кожного тесту та розгорнутого
runner report, перша невдала умова зупиняє процес.
