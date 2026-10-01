#include "board.h"
#include "bus.h"
#include "../Devices/imu.h"
#include <stdio.h>
static ADC_HandleTypeDef adc;
static kg_imu imu;
static kg_motion motion;
static uint16_t light;
static bool imu_ok, adc_ok, pir_level;
static kg_status imu_status;
static uint32_t imu_time, adc_time, pir_time, heartbeat_time, sample_time;
static char tx[256];
static void adc_setup(void)
{
    __HAL_RCC_ADC_CLK_ENABLE();
    GPIO_InitTypeDef pin = {0};
    pin.Pin = GPIO_PIN_0;
    pin.Mode = GPIO_MODE_ANALOG;
    HAL_GPIO_Init(GPIOA, &pin);
    adc.Instance = ADC1;
    adc.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
    adc.Init.Resolution = ADC_RESOLUTION_12B;
    adc.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    adc.Init.ScanConvMode = ADC_SCAN_DISABLE;
    adc.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
    adc.Init.NbrOfConversion = 1;
    adc.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    adc.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    adc.Init.Overrun = ADC_OVR_DATA_PRESERVED;
    adc.Init.SamplingTimeCommon1 = ADC_SAMPLETIME_160CYCLES_5;
    adc.Init.SamplingTimeCommon2 = ADC_SAMPLETIME_160CYCLES_5;
    adc.Init.TriggerFrequencyMode = ADC_TRIGGER_FREQ_LOW;
    if (HAL_ADC_Init(&adc) != HAL_OK) Error_Handler();
    ADC_ChannelConfTypeDef channel = {0};
    channel.Channel = ADC_CHANNEL_0;
    channel.Rank = ADC_REGULAR_RANK_1;
    channel.SamplingTime = ADC_SAMPLINGTIME_COMMON_1;
    if (HAL_ADC_ConfigChannel(&adc, &channel) != HAL_OK) Error_Handler();
    if (HAL_ADCEx_Calibration_Start(&adc) != HAL_OK) Error_Handler();
}
static bool read_light(void)
{
    HAL_GPIO_WritePin(GPIOA, KG_ADC_MARK_PIN, GPIO_PIN_SET);
    bool ok = HAL_ADC_Start(&adc) == HAL_OK;
    if (ok) ok = HAL_ADC_PollForConversion(&adc, 2) == HAL_OK;
    if (ok) light = (uint16_t)HAL_ADC_GetValue(&adc);
    HAL_ADC_Stop(&adc);
    HAL_GPIO_WritePin(GPIOA, KG_ADC_MARK_PIN, GPIO_PIN_RESET);
    return ok;
}
void kg_platform_start(void)
{
    GPIO_InitTypeDef pin = {0};
    pin.Pin = KG_PIR_PIN;
    pin.Mode = GPIO_MODE_INPUT;
    pin.Pull = GPIO_PULLDOWN;
    HAL_GPIO_Init(GPIOA, &pin);
    pin.Pin = KG_ADC_MARK_PIN;
    pin.Mode = GPIO_MODE_OUTPUT_PP;
    pin.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &pin);
    adc_setup();
    imu.bus = kg_platform_bus();
    imu.address = 0x68;
    imu_status = kg_imu_start(&imu);
    if (imu_status == KG_OK) imu_status = kg_imu_read(&imu, &motion);
    imu_ok = imu_status == KG_OK;
    adc_ok = read_light();
    pir_level = HAL_GPIO_ReadPin(GPIOA, KG_PIR_PIN) == GPIO_PIN_SET;
    printf("BOOT project=KioskGuard lab=2\r\n");
    printf("SELFTEST device=MPU6050 status=%s id_expected=0x68 range_g=4\r\n",
           kg_status_name(imu_status));
    printf("SELFTEST device=LDR status=%s adc=%u\r\n", adc_ok ? "READABLE" : "ERROR", light);
    printf("SELFTEST device=PIR status=READABLE out=%u\r\n", pir_level);
    printf("STM32C031C6 ready\r\n");
    imu_time = adc_time = pir_time = HAL_GetTick();
}
void kg_platform_run(void)
{
    uint32_t now = HAL_GetTick();
    if ((uint32_t)(now - pir_time) >= 20) {
        pir_time = now;
        pir_level = HAL_GPIO_ReadPin(GPIOA, KG_PIR_PIN) == GPIO_PIN_SET;
    }
    if ((uint32_t)(now - adc_time) >= 50) {
        adc_time = now;
        adc_ok = read_light();
    }
    if ((uint32_t)(now - imu_time) >= (imu_ok ? 20U : 1000U)) {
        imu_time = now;
        imu_status = imu_ok ? kg_imu_read(&imu, &motion) : kg_imu_start(&imu);
        if (imu_status == KG_OK && !imu_ok) imu_status = kg_imu_read(&imu, &motion);
        imu_ok = imu_status == KG_OK;
    }
    if ((uint32_t)(now - heartbeat_time) >= 1000) {
        heartbeat_time = now;
        HAL_GPIO_TogglePin(Led_GPIO_Port, Led_Pin);
    }
    if (huart2.gState == HAL_UART_STATE_READY && (uint32_t)(now - sample_time) >= 500) {
        sample_time = now;
        int n = snprintf(tx, sizeof(tx),
            "SAMPLE t=%lu ax=%ld ay=%ld az=%ld adc=%u pir=%u imu_ok=%u adc_ok=%u\r\n",
            (unsigned long)now, (long)motion.ax, (long)motion.ay, (long)motion.az,
            light, pir_level, imu_ok, adc_ok);
        HAL_UART_Transmit_IT(&huart2, (uint8_t *)tx, (uint16_t)n);
    }
}
