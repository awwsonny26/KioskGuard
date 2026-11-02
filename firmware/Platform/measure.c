#include "board.h"
#include "measure.h"
/* SysTick schedules at 1 ms; TIM3 measures short elapsed spans, including waits. */
void kg_measure_init(void) {
    __HAL_RCC_TIM3_CLK_ENABLE(); TIM3->CR1=0;
    TIM3->PSC=SystemCoreClock/1000000U-1U; TIM3->ARR=65535;
    TIM3->EGR=TIM_EGR_UG; TIM3->SR=0; TIM3->DIER=0; TIM3->CNT=0;
    TIM3->CR1=TIM_CR1_CEN;
}
uint16_t kg_perf_ticks(void) { return (uint16_t)TIM3->CNT; }
uint32_t kg_now_us(void) { return HAL_GetTick() * 1000U; }
uint32_t kg_now_ms(void) { return HAL_GetTick(); }
extern uint32_t _estack, _Min_Stack_Size;
void kg_stack_paint(void)
{
    uint32_t *p=(uint32_t *)((uintptr_t)&_estack-(uintptr_t)&_Min_Stack_Size);
    uint32_t *limit=(uint32_t *)__get_MSP()-16;
    while(p<limit)*p++=0xdeadbeefU;
}
unsigned kg_stack_used(void)
{
    uint32_t *p=(uint32_t *)((uintptr_t)&_estack-(uintptr_t)&_Min_Stack_Size);
    while(p<&_estack && *p==0xdeadbeefU)++p;
    return (unsigned)((uintptr_t)&_estack-(uintptr_t)p);
}
