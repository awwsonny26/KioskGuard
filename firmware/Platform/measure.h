#ifndef KG_MEASURE_H
#define KG_MEASURE_H
#include <stdint.h>
void kg_measure_init(void);
uint16_t kg_perf_ticks(void);
uint32_t kg_now_us(void);
uint32_t kg_now_ms(void);
void kg_stack_paint(void);
unsigned kg_stack_used(void);
unsigned kg_heap_peak(void);
#endif
