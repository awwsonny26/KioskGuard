#include "board.h"
#include "bus.h"
#include "../Devices/imu.h"
#include <stdio.h>
#include <string.h>
#include "../App/kiosk.h"
#include "../App/line.h"
#include "../App/audit.h"
#include "../App/cadence.h"
#include "measure.h"
#include "nvflash.h"
#include "version.h"
static ADC_HandleTypeDef adc;
static kg_imu imu;
static kg_motion motion;
static uint16_t light;
static bool imu_ok, adc_ok, pir_level;
static kg_status imu_status;
static uint32_t nominal_imu_period=20000;
static uint32_t sample_time;
static char tx[256];
static kg_kiosk kiosk;
static kg_line command_line;
static kg_button button;
static uint8_t rx_byte, rx_ring[128];
static volatile unsigned rx_head, rx_tail;
static volatile bool rx_overflow;
static volatile uint32_t rx_times[128], rx_dropped, rx_errors;
static volatile uint32_t rx_bytes, rx_rearm_fail, rx_retry_attempts;
static volatile uint32_t first_rearm_status, first_rearm_state, first_rearm_error;
static volatile bool rx_rearm_pending, first_rearm_recorded;
static uint32_t next_rx_retry;
static char rx_last_line[64];
static unsigned rx_last_dispatch;
static bool rx_diag_pending;
static kg_audit audit;
enum {KG_TASK_CMD,KG_TASK_PIR,KG_TASK_ADC,KG_TASK_IMU,KG_TASK_POLICY,KG_TASK_NV,KG_TASK_HEART,KG_TASK_TX,KG_TASK_TASKS};
static const char *const task_names[]={"command","pir","adc","imu","policy","audit","heartbeat","output"};
static kg_cadence clocks[KG_TASK_TASKS];
static uint32_t range_errors;
static uint32_t busy_us, load_start, load_permille, loop_last, loop_min, loop_max, loop_count;
static uint64_t loop_sum;
static uint32_t command_max, command_count, event_age_max;
static unsigned report_line, export_slot;
static bool stats_pending, audit_pending, exporting;
static bool imu_fresh_pending, adc_fresh_pending;
static bool timer_checked, clock_pending, clock_ready;
static uint32_t clock_elapsed_us, clock_elapsed_ms;
static uint32_t boot_clock, boot_tick, first_imu_us, first_adc_us, first_pir_us;

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
/* Bounded recovery; no logging inside the IRQ. Save the first failed HAL call. */
static void rx_arm(void)
{
    uint32_t state=huart2.RxState, error=huart2.ErrorCode;
    HAL_StatusTypeDef status=HAL_UART_Receive_IT(&huart2, &rx_byte, 1);
    if(status==HAL_OK) rx_rearm_pending=false;
    else {
        ++rx_rearm_fail; rx_rearm_pending=true;
        if(!first_rearm_recorded) {
            first_rearm_status=status; first_rearm_state=state; first_rearm_error=error;
            first_rearm_recorded=true;
        }
    }
}
static void rx_retry(uint32_t now)
{
    if (!rx_rearm_pending || (int32_t)(now-next_rx_retry)<0) return;
    next_rx_retry=now+10U;
    uint32_t mask=__get_PRIMASK(); __disable_irq();
    if (rx_rearm_pending && huart2.RxState==HAL_UART_STATE_READY) {
        ++rx_retry_attempts; rx_arm();
    }
    __set_PRIMASK(mask);
}
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *uart)
{
    if (uart != &huart2) return;
    ++rx_bytes;
    unsigned next_head = (rx_head + 1) % sizeof(rx_ring);
    if (next_head == rx_tail) {rx_overflow = true;++rx_dropped;}
    else {
        rx_ring[rx_head] = rx_byte;
        rx_times[rx_head] = kg_now_us();
        __DMB();
        rx_head = next_head;
    }
    rx_arm();
}
void HAL_UART_ErrorCallback(UART_HandleTypeDef *uart)
{
    if (uart != &huart2) return;
    rx_overflow = true;++rx_errors;
    __HAL_UART_CLEAR_OREFLAG(&huart2);
    rx_arm();
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
    for (unsigned budget=0; budget<32 && rx_tail != rx_head; ++budget) {
        unsigned char byte = rx_ring[rx_tail];
        uint32_t arrived=rx_times[rx_tail];
        __DMB();
        rx_tail = (rx_tail+1) % sizeof(rx_ring);
        kg_line_result result = kg_line_feed(&command_line, byte);
        if (result == KG_LINE_REJECTED) {
            kg_emit(&kiosk, now, KG_COMMAND_REJECTED, in, 3);
        } else if (result == KG_LINE_READY) {
            uint32_t latency=kg_now_us()-arrived;
            if(latency>command_max)command_max=latency;
            ++command_count;
            if (strcmp(command_line.text,"rxdiag")!=0) {
                memcpy(rx_last_line,command_line.text,sizeof(rx_last_line));
                rx_last_line[sizeof(rx_last_line)-1]=0;
                rx_last_dispatch = strcmp(command_line.text,"stats")==0 ? 1 :
                    strcmp(command_line.text,"audit status")==0 ? 2 :
                    strcmp(command_line.text,"audit save")==0 ? 3 :
                    strcmp(command_line.text,"service on")==0 ? 4 :
                    strcmp(command_line.text,"service off")==0 ? 5 : 0;
            }
            if (strcmp(command_line.text,"rxdiag")==0) rx_diag_pending=true;
            else if (strcmp(command_line.text,"stats")==0) {stats_pending=true;report_line=0;}
            else if (strcmp(command_line.text,"audit status")==0) audit_pending=true;
            else if (strcmp(command_line.text,"audit save")==0) {kg_audit_request(&audit);audit_pending=true;}
            else if (strcmp(command_line.text,"audit retry")==0) {
                kg_audit_totals pending=audit.totals;
                kg_audit_mount(&audit,kg_nvflash_io());
                audit.totals=pending;kg_audit_request(&audit);audit_pending=true;
            }
            else if (strcmp(command_line.text,"export")==0) {exporting=true;export_slot=0;}
            else if (strcmp(command_line.text,"rate 5")==0 || strcmp(command_line.text,"rate 10")==0 ||
                     strcmp(command_line.text,"rate 20")==0) {
                unsigned period=strcmp(command_line.text,"rate 5")==0?5000:
                    strcmp(command_line.text,"rate 10")==0?10000:20000;
                nominal_imu_period=period;
                if(imu_ok) {
                    clocks[KG_TASK_IMU].period=period;clocks[KG_TASK_IMU].deadline=period;
                    clocks[KG_TASK_IMU].next=kg_now_us()+period;
                    clocks[KG_TASK_IMU].started=false;
                }
                kg_emit(&kiosk,now,KG_DIAGNOSTIC,in,(int32_t)period);
            }
            else if (strcmp(command_line.text, "probe") == 0) capture_pending = true;
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
    kg_measure_init();
    boot_clock=kg_now_us();boot_tick=HAL_GetTick();
    /* Verify the chosen time source before any bus timeout depends on it.
     * This is a clock self-test, not a simulated flash delay. */
    while((uint32_t)(HAL_GetTick()-boot_tick)<3U) {}
    clock_ready=(uint32_t)(kg_now_us()-boot_clock)>=1000U;
    if(!clock_ready) {
        printf("BOOT project=KioskGuard lab=3 source=%s CLOCK_UNSUPPORTED TIM3\r\n",KG_VERSION);
        return;
    }
    GPIO_InitTypeDef pin = {0};
    pin.Pin = User_Button_Pin;
    pin.Mode = GPIO_MODE_INPUT;
    pin.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(User_Button_GPIO_Port, &pin);
    pin.Pin = KG_PIR_PIN;
    pin.Mode = GPIO_MODE_INPUT;
    pin.Pull = GPIO_PULLDOWN;
    HAL_GPIO_Init(GPIOA, &pin);
    pin.Pin = KG_ADC_MARK_PIN | GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8;
    pin.Mode = GPIO_MODE_OUTPUT_PP;
    pin.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &pin);
    adc_setup();
    imu.bus = kg_platform_bus();
    imu.address = 0x68;
    imu_status = kg_imu_start(&imu);
    if (imu_status == KG_OK) imu_status = kg_imu_read(&imu, &motion);
    imu_ok = imu_status == KG_OK;
    first_imu_us=kg_now_us();
    adc_ok = read_light();
    first_adc_us=kg_now_us();
    pir_level = HAL_GPIO_ReadPin(GPIOA, KG_PIR_PIN) == GPIO_PIN_SET;
    first_pir_us=kg_now_us();
    printf("BOOT project=KioskGuard lab=3 source=%s timer=SysTick_TIM3_span_CANDIDATE\r\n",KG_VERSION);
    printf("SELFTEST device=MPU6050 status=%s id_expected=0x68 range_g=4\r\n",
           kg_status_name(imu_status));
    printf("SELFTEST device=LDR status=%s adc=%u\r\n", adc_ok ? "READABLE" : "ERROR", light);
    printf("SELFTEST device=PIR status=READABLE out=%u\r\n", pir_level);
    printf("STM32C031C6 ready\r\n");
    uint32_t start=kg_now_us();
    const uint32_t periods[KG_TASK_TASKS]={1000,20000,50000,20000,5000,1000,1000000,1000};
    for(unsigned i=0;i<KG_TASK_TASKS;++i) {clocks[i].period=periods[i];clocks[i].deadline=periods[i];clocks[i].next=start+periods[i];}
    if(!imu_ok) {clocks[KG_TASK_IMU].period=1000000;clocks[KG_TASK_IMU].deadline=1000000;clocks[KG_TASK_IMU].next=start+1000000;}
    load_start=loop_last=start;
    kg_audit_mount(&audit,kg_nvflash_io());
    printf("AUDIT_BOOT valid=%u gen=%lu invalid=%lu failed=%u imu_us=%lu adc_us=%lu pir_us=%lu\r\n",
        audit.found,(unsigned long)audit.committed,(unsigned long)audit.invalid,audit.failed,
        (unsigned long)first_imu_us,(unsigned long)first_adc_us,(unsigned long)first_pir_us);
    kg_kiosk_init(&kiosk);
    kg_input in = inputs(true, true);
    kg_request_baseline(&kiosk, kg_now_ms(), &in);
    rx_arm();
}
static void output(uint32_t now)
{
    if (huart2.gState != HAL_UART_STATE_READY) return;
    kg_event event;
    int n = 0;
    if(clock_pending) {
        clock_pending=false;
        n=snprintf(tx,sizeof(tx),"CLOCK_CHECK coarse_us=%lu hal_ms=%lu supported=%u\r\n",
            (unsigned long)clock_elapsed_us,(unsigned long)clock_elapsed_ms,clock_elapsed_us!=0);
    } else if (rx_diag_pending) {
        rx_diag_pending=false;
        n=snprintf(tx,sizeof(tx),"RXDIAG bytes=%lu errors=%lu drop=%lu armfail=%lu retry=%lu pending=%u recorded=%u first=%lu state=%lu err=%lu dispatch=%u line=%s\r\n",
            (unsigned long)rx_bytes,(unsigned long)rx_errors,(unsigned long)rx_dropped,
            (unsigned long)rx_rearm_fail,(unsigned long)rx_retry_attempts,rx_rearm_pending,first_rearm_recorded,
            (unsigned long)first_rearm_status,(unsigned long)first_rearm_state,
            (unsigned long)first_rearm_error,rx_last_dispatch,rx_last_line);
    } else if (audit_pending) {
        audit_pending=false;
        n=snprintf(tx,sizeof(tx),"AUDIT gen=%lu committed=%lu near=%lu shock=%lu moved=%lu light=%lu errors=%lu service_changes=%lu busy=%u dirty=%u failed=%u invalid=%lu coalesced=%lu\r\n",
          (unsigned long)audit.generation,(unsigned long)audit.committed,
          (unsigned long)audit.totals.near,(unsigned long)audit.totals.shocks,
          (unsigned long)audit.totals.moves,(unsigned long)audit.totals.lights,
          (unsigned long)audit.totals.errors,(unsigned long)audit.totals.services,
          audit.busy,audit.dirty,audit.failed,(unsigned long)audit.invalid,(unsigned long)audit.coalesced);
    } else if (stats_pending) {
        if(report_line<KG_TASK_TASKS) {
            unsigned i=report_line++; kg_cadence *c=&clocks[i];
            n=snprintf(tx,sizeof(tx),"STAT name=%s n=%lu min_us=%lu avg_us=%lu max_us=%lu jitter_min_ms=%ld jitter_max_ms=%ld skipped=%lu late=%lu\r\n",task_names[i],(unsigned long)c->runs,(unsigned long)c->min_us,(unsigned long)(c->runs?c->sum_us/c->runs:0),(unsigned long)c->max_us,(long)(c->jitter_min/1000),(long)(c->jitter_max/1000),(unsigned long)c->skipped,(unsigned long)c->late);
        } else {
            stats_pending=false;
            n=snprintf(tx,sizeof(tx),"SYSTEM foreground_permille=%lu stack_used=%u heap_peak=%u range_errors=%lu cpu=polling\r\n",(unsigned long)load_permille,kg_stack_used(),kg_heap_peak(),(unsigned long)range_errors);
        }
    } else if (kg_event_pop(&kiosk, &event)) {
        kg_audit_event(&audit,&event,kiosk.base_adc);
        uint32_t age=now-event.time;if(age>event_age_max)event_age_max=age;
        n = snprintf(tx, sizeof(tx),
            "EVENT seq=%lu t=%lu type=%s mode=%s severity=%s ax=%ld ay=%ld az=%ld adc=%u metric=%ld\r\n",
            (unsigned long)event.number, (unsigned long)event.time,
            kg_event_name(event.type), event.service ? "SERVICE" : "CONTROL",
            kg_severity(&event), (long)event.motion.ax, (long)event.motion.ay,
            (long)event.motion.az, event.adc, (long)event.value);
    } else if (exporting) {
        uint8_t record[64];
        unsigned offset=export_slot*64;
        bool ok=audit.io.read(audit.io.context,offset,record,sizeof(record));
        n=snprintf(tx,sizeof(tx),"EXPORT slot=%u ok=%u data=",export_slot,ok);
        if(ok)for(unsigned i=0;i<sizeof(record);++i)
            n+=snprintf(tx+n,sizeof(tx)-(unsigned)n,"%02x",record[i]);
        n+=snprintf(tx+n,sizeof(tx)-(unsigned)n,"\r\n");
        if(++export_slot==64)exporting=false;
    } else if (capture_pending) {
        capture_pending = false;
        uint8_t id = 0;
        kg_status status = imu.bus.read(imu.bus.context, imu.address, 0x75, &id, 1);
        kg_motion sample = {0};
        if (status == KG_OK) status = kg_imu_read(&imu, &sample);
        n = snprintf(tx, sizeof(tx), "CAPTURE t=%lu addr=0x%02x id=0x%02x status=%s raw=",
            (unsigned long)kg_now_ms(), imu.address, id, kg_status_name(status));
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
    if(!clock_ready) {__WFI();return;}
    uint32_t cycle=kg_now_us(), gap=cycle-loop_last;
    loop_last=cycle;loop_sum+=gap;if(!loop_count || gap<loop_min)loop_min=gap;
    ++loop_count;if(gap>loop_max)loop_max=gap;
    uint32_t now=kg_now_ms();
    rx_retry(now);
    if(!timer_checked && (uint32_t)(HAL_GetTick()-boot_tick)>=100) {
        timer_checked=true;
        /* This check covers the coarse scheduler clock, not TIM3. */
        clock_elapsed_us=cycle-boot_clock;
        clock_elapsed_ms=HAL_GetTick()-boot_tick;clock_pending=true;
    }
    for(unsigned id=0;id<KG_TASK_TASKS;++id) {
        uint32_t begin=kg_now_us();
        if(!kg_cadence_due(&clocks[id],begin))continue;
        uint16_t perf_begin=kg_perf_ticks();
        uint32_t perf_ms=HAL_GetTick();
        kg_input in=inputs(false,false);
        bool work=true;
        switch(id) {
        case KG_TASK_CMD:work=rx_overflow || rx_tail!=rx_head;process_commands(now,&in);break;
        case KG_TASK_PIR:pir_level=HAL_GPIO_ReadPin(GPIOA,KG_PIR_PIN)==GPIO_PIN_SET;break;
        case KG_TASK_ADC:adc_ok=read_light();adc_fresh_pending=true;break;
        case KG_TASK_IMU:
            GPIOA->BSRR=GPIO_PIN_6;
            bool previously_ok=imu_ok;
            imu_status=imu_ok?kg_imu_read(&imu,&motion):kg_imu_start(&imu);
            if(imu_status==KG_OK && !imu_ok)imu_status=kg_imu_read(&imu,&motion);
            imu_ok=imu_status==KG_OK;imu_fresh_pending=true;
            if(previously_ok!=imu_ok) {
                clocks[KG_TASK_IMU].period=imu_ok?nominal_imu_period:1000000;
                clocks[KG_TASK_IMU].deadline=clocks[KG_TASK_IMU].period;
                clocks[KG_TASK_IMU].next=kg_now_us()+clocks[KG_TASK_IMU].period;
                clocks[KG_TASK_IMU].started=false;
            }
            GPIOA->BSRR=(uint32_t)GPIO_PIN_6<<16;break;
        case KG_TASK_POLICY:
            in=inputs(imu_fresh_pending,adc_fresh_pending);
            imu_fresh_pending=adc_fresh_pending=false;
            now=kg_now_ms();
            if(kg_button_press(&button,HAL_GPIO_ReadPin(User_Button_GPIO_Port,User_Button_Pin)==GPIO_PIN_RESET,now))
                kg_request_baseline(&kiosk,now,&in);
            bool approach=in.pir && !kiosk.pir_previous;
            kg_kiosk_step(&kiosk,now,&in);
            if(approach) {
                GPIOA->BSRR=GPIO_PIN_7;
                __NOP();__NOP();
                GPIOA->BSRR=(uint32_t)GPIO_PIN_7<<16;
            }
            break;
        case KG_TASK_NV:
            work=!audit.failed && (audit.busy || audit.dirty);
            if(work) {GPIOA->BSRR=GPIO_PIN_8;kg_audit_step(&audit);GPIOA->BSRR=(uint32_t)GPIO_PIN_8<<16;}
            break;
        case KG_TASK_HEART:++tick_number;tick_pending=true;HAL_GPIO_TogglePin(Led_GPIO_Port,Led_Pin);break;
        case KG_TASK_TX:work=huart2.gState==HAL_UART_STATE_READY;output(kg_now_ms());break;
        default:break;
        }
        uint16_t perf_end=kg_perf_ticks();
        uint32_t ms_span=HAL_GetTick()-perf_ms;
        uint32_t elapsed=(uint16_t)(perf_end-perf_begin);
        if(ms_span>=60U) { ++range_errors; elapsed=ms_span*1000U; }
        kg_cadence_finish(&clocks[id],elapsed);if(work)busy_us+=elapsed;
    }
    uint32_t elapsed=kg_now_us()-load_start;
    if(elapsed>=1000000) {
        load_permille=(uint32_t)((uint64_t)busy_us*1000/elapsed);
        busy_us=0;load_start=kg_now_us();
    }
    /* Diagnostic candidate: busy polling instead of WFI. */
}
