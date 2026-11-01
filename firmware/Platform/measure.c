#include "board.h"
#include "measure.h"
/* Isolated diagnostic candidate: 1 ms resolution, not CPU/WCET profiling. */
void kg_measure_init(void) {}
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
