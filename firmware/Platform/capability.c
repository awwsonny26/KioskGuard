/* Isolated diagnostic image. It does not execute the KioskGuard task loop. */
#include "board.h"
#include "measure.h"
#include "version.h"
#include "imu.h"
#include "audit.h"
#include <stdio.h>
#include <string.h>

#define AUDIT_BASE 0x08007000U
extern volatile uint32_t kg_probe_irq_count;
extern volatile uint32_t kg_probe_irq_samples[8][4];
extern kg_bus kg_platform_bus(void);
static uint8_t rx_byte;
static volatile uint8_t rx_ring[64];
static volatile unsigned rx_head,rx_tail,rx_drop,rx_errors;

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *uart)
{
    if(uart!=&huart2)return;
    unsigned next=(rx_head+1U)%sizeof(rx_ring);
    if(next==rx_tail)++rx_drop;
    else {rx_ring[rx_head]=rx_byte;rx_head=next;}
    HAL_UART_Receive_IT(&huart2,&rx_byte,1);
}
void HAL_UART_ErrorCallback(UART_HandleTypeDef *uart)
{
    if(uart!=&huart2)return;
    ++rx_errors;__HAL_UART_CLEAR_OREFLAG(uart);
    HAL_UART_Receive_IT(&huart2,&rx_byte,1);
}
static void marker_init(void)
{
    GPIO_InitTypeDef p={0};
    p.Pin=GPIO_PIN_4|GPIO_PIN_6|GPIO_PIN_7|GPIO_PIN_8;
    p.Mode=GPIO_MODE_OUTPUT_PP;p.Pull=GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA,&p);
}
static void timer_sample(const char *phase,uint32_t start)
{
    uint32_t tick=HAL_GetTick(),cnt=TIM3->CNT,sr=TIM3->SR;
    printf("PROBE_CLOCK phase=%s ms=%lu cnt=%lu sr=%lx dier=%lx irq=%lu\r\n",
        phase,(unsigned long)(tick-start),(unsigned long)cnt,(unsigned long)sr,
        (unsigned long)TIM3->DIER,(unsigned long)kg_probe_irq_count);
}
static void timer_phase(const char *phase)
{
    uint32_t start=HAL_GetTick(),next=0;
    while((uint32_t)(HAL_GetTick()-start)<350U) {
        if((uint32_t)(HAL_GetTick()-start)>=next) {
            timer_sample(phase,start);next+=50;
            GPIOA->ODR^=GPIO_PIN_7;
        }
    }
    timer_sample(phase,start);
}
static unsigned non_ff(unsigned page)
{
    const volatile uint8_t *p=(const volatile uint8_t *)(AUDIT_BASE+page*KG_AUDIT_PAGE);
    unsigned count=0;
    for(unsigned i=0;i<KG_AUDIT_PAGE;++i)count+=p[i]!=255;
    return count;
}
static bool diag_read(void *ctx,unsigned offset,void *out,size_t n)
{
    (void)ctx;
    if(offset+n>2*KG_AUDIT_PAGE)return false;
    memcpy(out,(const void *)(AUDIT_BASE+offset),n);return true;
}
static bool diag_erase(void *ctx,unsigned page)
{
    (void)ctx;
    if(page>1)return false;
    uint32_t before_sr=FLASH->SR,before_cr=FLASH->CR;
    unsigned before=non_ff(page);
    HAL_StatusTypeDef unlock=HAL_FLASH_Unlock();
    FLASH_EraseInitTypeDef e={.TypeErase=FLASH_TYPEERASE_PAGES,.Page=14+page,.NbPages=1};
    uint32_t page_error=~0U;
    GPIOA->BSRR=GPIO_PIN_8;
    uint32_t start=HAL_GetTick();
    HAL_StatusTypeDef result=unlock==HAL_OK?HAL_FLASHEx_Erase(&e,&page_error):unlock;
    uint32_t elapsed=HAL_GetTick()-start,error=HAL_FLASH_GetError();
    uint32_t after_sr=FLASH->SR,after_cr=FLASH->CR;
    GPIOA->BSRR=(uint32_t)GPIO_PIN_8<<16;
    HAL_FLASH_Lock();
    unsigned after=non_ff(page);
    printf("PROBE_ERASE page=%u unlock=%u status=%u error=%lx page_error=%lx ms=%lu\r\n",
        14+page,(unsigned)unlock,(unsigned)result,(unsigned long)error,
        (unsigned long)page_error,(unsigned long)elapsed);
    printf("PROBE_PAGE page=%u bytes=%u nonff_before=%u nonff_after=%u sr_before=%lx cr_before=%lx sr_after=%lx cr_after=%lx\r\n",
        14+page,KG_AUDIT_PAGE,before,after,(unsigned long)before_sr,(unsigned long)before_cr,
        (unsigned long)after_sr,(unsigned long)after_cr);
    /* HAL_OK is insufficient: require the entire actual page to be erased. */
    return unlock==HAL_OK && result==HAL_OK && after==0;
}
static bool diag_program(void *ctx,unsigned offset,const uint8_t bytes[8])
{
    (void)ctx;
    if(offset%8 || offset+8>2*KG_AUDIT_PAGE)return false;
    uint64_t value;memcpy(&value,bytes,8);
    HAL_StatusTypeDef unlock=HAL_FLASH_Unlock();
    GPIOA->BSRR=GPIO_PIN_8;
    HAL_StatusTypeDef result=unlock==HAL_OK?
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD,AUDIT_BASE+offset,value):unlock;
    uint32_t error=HAL_FLASH_GetError(),sr=FLASH->SR,cr=FLASH->CR;
    GPIOA->BSRR=(uint32_t)GPIO_PIN_8<<16;
    HAL_FLASH_Lock();
    bool equal=memcmp((const void *)(AUDIT_BASE+offset),bytes,8)==0;
    printf("PROBE_PROGRAM offset=%u unlock=%u status=%u error=%lx sr=%lx cr=%lx equal8=%u\r\n",
        offset,(unsigned)unlock,(unsigned)result,(unsigned long)error,
        (unsigned long)sr,(unsigned long)cr,equal);
    return unlock==HAL_OK && result==HAL_OK && equal;
}
static void storage_probe(void)
{
    printf("PROBE_STORAGE base=%lx page_bytes=%u hal_page_bytes=%lu optr=%lx\r\n",
        (unsigned long)AUDIT_BASE,KG_AUDIT_PAGE,(unsigned long)FLASH_PAGE_SIZE,(unsigned long)FLASH->OPTR);
    bool page0=diag_erase(NULL,0),page1=diag_erase(NULL,1);
    if(!page0 || !page1) {
        printf("PROBE_STORAGE result=ERASE_READBACK_FAILED program_crc=SKIPPED\r\n");
        return;
    }
    kg_audit_io io={NULL,diag_read,diag_program,diag_erase};
    kg_audit a,remounted;
    bool mount=kg_audit_mount(&a,io);
    a.totals.near=1;a.totals.services=2;a.totals.last_ms=HAL_GetTick();
    kg_audit_request(&a);
    for(unsigned i=0;i<10 && !a.failed && (a.busy || a.dirty);++i)kg_audit_step(&a);
    bool remount=kg_audit_mount(&remounted,io);
    printf("PROBE_CRC mount=%u failed=%u committed=%lu remount=%u found=%u generation=%lu near=%lu service=%lu invalid=%lu\r\n",
        mount,a.failed,(unsigned long)a.committed,remount,remounted.found,
        (unsigned long)remounted.committed,(unsigned long)remounted.totals.near,
        (unsigned long)remounted.totals.services,(unsigned long)remounted.invalid);
}
static void bus_probe(uint8_t address)
{
    kg_imu imu={.bus=kg_platform_bus(),.address=address};
    kg_motion motion={0};
    GPIOA->BSRR=GPIO_PIN_6;
    uint32_t start=HAL_GetTick();
    kg_status init=kg_imu_start(&imu);
    kg_status read=init==KG_OK?kg_imu_read(&imu,&motion):init;
    uint32_t elapsed=HAL_GetTick()-start;
    GPIOA->BSRR=(uint32_t)GPIO_PIN_6<<16;
    printf("PROBE_I2C address=%x init=%u read=%u hal_ms=%lu ax=%ld ay=%ld az=%ld\r\n",
        address,(unsigned)init,(unsigned)read,(unsigned long)elapsed,
        (long)motion.ax,(long)motion.ay,(long)motion.az);
}
void kg_capability_pilot(void)
{
    marker_init();
    printf("BOOT project=KioskGuard source=%s mode=CAPABILITY1 not_final_application\r\n",KG_VERSION);
    printf("PROBE_LIMIT timeout_clock=HAL_SysTick_ms physical_power=UNTESTED\r\n");
    kg_measure_init();
    timer_phase("irq_off");
    TIM3->SR=0;HAL_NVIC_ClearPendingIRQ(TIM3_IRQn);
    HAL_NVIC_EnableIRQ(TIM3_IRQn);TIM3->DIER=TIM_DIER_UIE;
    timer_phase("irq_guarded8");
    TIM3->DIER=0;HAL_NVIC_DisableIRQ(TIM3_IRQn);
    for(unsigned i=0;i<kg_probe_irq_count && i<8;++i)
        printf("PROBE_IRQ index=%u tick=%lu cnt=%lu sr_in=%lx sr_after_clear=%lx\r\n",i,
            (unsigned long)kg_probe_irq_samples[i][0],(unsigned long)kg_probe_irq_samples[i][1],
            (unsigned long)kg_probe_irq_samples[i][2],(unsigned long)kg_probe_irq_samples[i][3]);
    bus_probe(0x68);bus_probe(0x69);bus_probe(0x68);
    storage_probe();
    printf("PROBE_UART enter_short_text interval_ms=1000 duration_ms=20000 irq_timer=OFF\r\n");
    HAL_UART_Receive_IT(&huart2,&rx_byte,1);
    uint32_t start=HAL_GetTick(),next=1000;
    while((uint32_t)(HAL_GetTick()-start)<20000U) {
        while(rx_tail!=rx_head) {
            unsigned byte=rx_ring[rx_tail];rx_tail=(rx_tail+1U)%sizeof(rx_ring);
            printf("PROBE_RX byte=%u ms=%lu\r\n",byte,(unsigned long)(HAL_GetTick()-start));
        }
        if((uint32_t)(HAL_GetTick()-start)>=next) {
            GPIOA->ODR^=GPIO_PIN_7;
            printf("PROBE_TICK ms=%lu drop=%u rx_error=%u\r\n",
                (unsigned long)(HAL_GetTick()-start),rx_drop,rx_errors);
            next+=1000;
        }
        __WFI();
    }
    HAL_UART_AbortReceive(&huart2);
    printf("PROBE_DONE application_scenarios=NOT_RUN\r\n");
}
