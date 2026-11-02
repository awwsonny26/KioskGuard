/* Isolated MCU peripheral probe; not an application or a persistence substitute. */
#include "board.h"
#include <stdio.h>
#include <stdint.h>
#define BASE 0x08007000U
void kg_capability_pilot(void)
{
    HAL_Delay(2500);
    volatile uint32_t *const word=(volatile uint32_t *)BASE;
    __HAL_RCC_TIM3_CLK_ENABLE();
    TIM3->CR1=0;TIM3->PSC=SystemCoreClock/1000000U-1U;
    TIM3->ARR=65535;TIM3->EGR=TIM_EGR_UG;TIM3->SR=0;TIM3->DIER=0;TIM3->CR1=TIM_CR1_CEN;
    uint32_t t0=HAL_GetTick(), c0=TIM3->CNT;
    while((uint32_t)(HAL_GetTick()-t0)<30U){}
    uint32_t t1=HAL_GetTick(),c1=TIM3->CNT;
    printf("PROBE_TIMER elapsed_ms=%lu cnt0=%lu cnt1=%lu delta16=%lu sr=%lx\r\n",(unsigned long)(t1-t0),(unsigned long)c0,(unsigned long)c1,(unsigned long)((c1-c0)&0xffff),(unsigned long)TIM3->SR);
    uint32_t before0=word[0],before1=word[1];
    HAL_StatusTypeDef unlocked=HAL_FLASH_Unlock();
    HAL_StatusTypeDef write=HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD,BASE,0x0123456789abcdefULL);
    uint32_t err=HAL_FLASH_GetError(),after0=word[0],after1=word[1];
    FLASH_EraseInitTypeDef erase={.TypeErase=FLASH_TYPEERASE_PAGES,.Page=14,.NbPages=1};uint32_t failed=0;
    HAL_StatusTypeDef erased=HAL_FLASHEx_Erase(&erase,&failed);
    uint32_t end0=word[0],end1=word[1];HAL_FLASH_Lock();
    printf("PROBE_FLASH before=%08lx:%08lx unlock=%u write=%u error=%lx after=%08lx:%08lx erase=%u end=%08lx:%08lx page_error=%lx\r\n",(unsigned long)before1,(unsigned long)before0,(unsigned)unlocked,(unsigned)write,(unsigned long)err,(unsigned long)after1,(unsigned long)after0,(unsigned)erased,(unsigned long)end1,(unsigned long)end0,(unsigned long)failed);
    printf("PROBE_END\r\n");
}
