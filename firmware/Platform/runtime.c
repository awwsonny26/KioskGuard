#include "board.h"
#include "bus.h"
#include "../Devices/imu.h"
#include <stdio.h>
#include <string.h>
#include "../App/kiosk.h"
#include "../App/line.h"
static ADC_HandleTypeDef adc;
static kg_imu imu;
static kg_motion motion;
static uint16_t light;
static bool imu_ok, adc_ok, pir_level;
static kg_status imu_status;
static uint32_t imu_time, adc_time, pir_time, heartbeat_time, sample_time;
static char tx[256];
static kg_kiosk kiosk;
static kg_line command_line;
static kg_button button;
static uint8_t rx_byte, rx_ring[128];
static volatile unsigned rx_head, rx_tail;
static volatile bool rx_overflow;
static bool capture_pending, tick_pending;
static unsigned tick_number;
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
static kg_input inputs(bool imu_fresh, bool adc_fresh)
{
    kg_input in = {0};
    in.motion = motion;
    in.adc = light;
    in.imu_valid = imu_ok;
    in.adc_valid = adc_ok;
    in.pir = pir_level;
    in.imu_fresh = imu_fresh;
    in.adc_fresh = adc_fresh;
    in.imu_status = imu_status;
    return in;
}
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *uart)
{
    if (uart != &huart2) return;
    unsigned next_head = (rx_head + 1) % sizeof(rx_ring);
    if (next_head == rx_tail) rx_overflow = true;
    else {
        rx_ring[rx_head] = rx_byte;
        __DMB();
        rx_head = next_head;
    }
    HAL_UART_Receive_IT(&huart2, &rx_byte, 1);
}
void HAL_UART_ErrorCallback(UART_HandleTypeDef *uart)
{
    if (uart != &huart2) return;
    rx_overflow = true;
    __HAL_UART_CLEAR_OREFLAG(&huart2);
    HAL_UART_Receive_IT(&huart2, &rx_byte, 1);
}
static void process_commands(uint32_t now, const kg_input *in)
{
    if (rx_overflow) {
        __disable_irq();
        rx_tail = rx_head;
        rx_overflow = false;
        __enable_irq();
        command_line.length = 0;
        command_line.rejected = true;
        kg_emit(&kiosk, now, KG_COMMAND_REJECTED, in, 2);
    }
    while (rx_tail != rx_head) {
        unsigned char byte = rx_ring[rx_tail];
        rx_tail = (rx_tail+1) % sizeof(rx_ring);
        kg_line_result result = kg_line_feed(&command_line, byte);
        if (result == KG_LINE_REJECTED) {
            kg_emit(&kiosk, now, KG_COMMAND_REJECTED, in, 3);
        } else if (result == KG_LINE_READY) {
            if (strcmp(command_line.text, "probe") == 0) capture_pending = true;
            else if (strcmp(command_line.text, "fault on") == 0 ||
                     strcmp(command_line.text, "fault off") == 0) {
                imu.address = strcmp(command_line.text, "fault on") == 0 ? 0x69 : 0x68;
                kg_emit(&kiosk, now, KG_DIAGNOSTIC, in, imu.address);
            } else kg_command(&kiosk, now, command_line.text, in);
        }
    }
}
void kg_platform_start(void)
{
    GPIO_InitTypeDef pin = {0};
    pin.Pin = User_Button_Pin;
    pin.Mode = GPIO_MODE_INPUT;
    pin.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(User_Button_GPIO_Port, &pin);
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
    kg_kiosk_init(&kiosk);
    kg_input in = inputs(true, true);
    kg_request_baseline(&kiosk, HAL_GetTick(), &in);
    HAL_UART_Receive_IT(&huart2, &rx_byte, 1);
}
static void output(uint32_t now)
{
    if (huart2.gState != HAL_UART_STATE_READY) return;
    kg_event event;
    int n = 0;
    if (kg_event_pop(&kiosk, &event)) {
        n = snprintf(tx, sizeof(tx),
            "EVENT seq=%lu t=%lu type=%s mode=%s severity=%s ax=%ld ay=%ld az=%ld adc=%u metric=%ld\r\n",
            (unsigned long)event.number, (unsigned long)event.time,
            kg_event_name(event.type), event.service ? "SERVICE" : "CONTROL",
            kg_severity(&event), (long)event.motion.ax, (long)event.motion.ay,
            (long)event.motion.az, event.adc, (long)event.value);
    } else if (capture_pending) {
        capture_pending = false;
        uint8_t id = 0;
        kg_status status = imu.bus.read(imu.bus.context, imu.address, 0x75, &id, 1);
        kg_motion sample = {0};
        if (status == KG_OK) status = kg_imu_read(&imu, &sample);
        n = snprintf(tx, sizeof(tx), "CAPTURE t=%lu addr=0x%02x id=0x%02x status=%s raw=",
            (unsigned long)HAL_GetTick(), imu.address, id, kg_status_name(status));
        for (unsigned i=0; i<14; ++i)
            n += snprintf(tx+n, sizeof(tx)-(unsigned)n, "%02x", sample.raw[i]);
        n += snprintf(tx+n, sizeof(tx)-(unsigned)n, " ax=%ld ay=%ld az=%ld temp=%ld\r\n",
            (long)sample.ax, (long)sample.ay, (long)sample.az, (long)sample.temperature);
    } else if (tick_pending) {
        tick_pending = false;
        n = snprintf(tx, sizeof(tx), "tick %u t=%lu\r\n", tick_number, (unsigned long)now);
    } else if ((uint32_t)(now-sample_time) >= 500) {
        sample_time = now;
        n = snprintf(tx, sizeof(tx),
            "SAMPLE t=%lu ax=%ld ay=%ld az=%ld adc=%u pir=%u imu=%s ready=%u base_adc=%u mode=%s\r\n",
            (unsigned long)now, (long)motion.ax, (long)motion.ay, (long)motion.az,
            light, pir_level, kg_status_name(imu_status), kiosk.ready, kiosk.base_adc,
            kiosk.service ? "SERVICE" : "CONTROL");
    }
    if (n > 0 && (unsigned)n < sizeof(tx))
        HAL_UART_Transmit_IT(&huart2, (uint8_t *)tx, (uint16_t)n);
}
void kg_platform_run(void)
{
    uint32_t now = HAL_GetTick();
    kg_input in = inputs(false, false);
    process_commands(now, &in);
    if ((uint32_t)(now-pir_time) >= 20) {
        pir_time = now;
        pir_level = HAL_GPIO_ReadPin(GPIOA, KG_PIR_PIN) == GPIO_PIN_SET;
    }
    bool adc_fresh = false, imu_fresh = false;
    if ((uint32_t)(now-adc_time) >= 50) {
        adc_time = now;
        adc_ok = read_light();
        adc_fresh = true;
    }
    if ((uint32_t)(now-imu_time) >= (imu_ok ? 20U : 1000U)) {
        imu_time = now;
        imu_status = imu_ok ? kg_imu_read(&imu, &motion) : kg_imu_start(&imu);
        if (imu_status == KG_OK && !imu_ok) imu_status = kg_imu_read(&imu, &motion);
        imu_ok = imu_status == KG_OK;
        imu_fresh = true;
    }
    now = HAL_GetTick();
    in = inputs(imu_fresh, adc_fresh);
    if (kg_button_press(&button,
            HAL_GPIO_ReadPin(User_Button_GPIO_Port, User_Button_Pin) == GPIO_PIN_RESET, now))
        kg_request_baseline(&kiosk, now, &in);
    kg_kiosk_step(&kiosk, now, &in);
    if ((uint32_t)(now-heartbeat_time) >= 1000) {
        heartbeat_time = now;
        ++tick_number;
        tick_pending = true;
        HAL_GPIO_TogglePin(Led_GPIO_Port, Led_Pin);
    }
    output(now);
}
