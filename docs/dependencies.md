# Версії та бібліотеки

ST HAL 1.0.1 з ЛР1. ADC/ADCEx/LL_ADC завантажено без змін з
https://github.com/STMicroelectronics/stm32c0xx-hal-driver/tree/24999b605d0b9da3e4675c4c884036ae724bb59f.
CMSIS із шаблону ЛР1 збережено без змін; контрольні хеші всього vendor дерева
зберігаються у vendor-sha256.txt після фінальної збірки.

GPIO для PIR/LED/markers, I2C для IMU, ADC для LDR, UART для журналу/команд.
RCC/FLASH/PWR/CORTEX потрібні startup і SysTick. DMA є залежністю типів HAL,
передач DMA немає. B1 — GPIO input pull-up; debounce виконується опитуванням.
Власні файли збираються з -Wall -Wextra -Werror; vendor тільки з -Wall.
Docker-файл фіксує образ та версії toolchain у фінальній конфігурації.
