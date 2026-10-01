#ifndef KG_BOARD_H
#define KG_BOARD_H
#include "main.h"
#include <stdbool.h>
#define KG_PIR_PIN GPIO_PIN_1
#define KG_ADC_MARK_PIN GPIO_PIN_4
extern I2C_HandleTypeDef hi2c1;
extern UART_HandleTypeDef huart2;
void kg_platform_start(void);
void kg_platform_run(void);
#endif
