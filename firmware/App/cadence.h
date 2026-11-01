#ifndef KG_CADENCE_H
#define KG_CADENCE_H
#include <stdbool.h>
#include <stdint.h>
typedef struct {
    uint32_t period, deadline, next, previous;
    uint32_t runs, skipped, late, min_us, max_us, max_release_us, release_us;
    int32_t jitter_min, jitter_max;
    uint32_t histogram[6];
    uint64_t sum_us;
    bool started;
} kg_cadence;
bool kg_cadence_due(kg_cadence *, uint32_t now);
void kg_cadence_finish(kg_cadence *, uint32_t elapsed);
#endif
