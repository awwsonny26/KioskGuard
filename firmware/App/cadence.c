#include "cadence.h"
bool kg_cadence_due(kg_cadence *c, uint32_t now)
{
    if((int32_t)(now-c->next)<0) return false;
    uint32_t slip=now-c->next, omitted=slip/c->period;
    c->skipped+=omitted;c->next+=(omitted+1)*c->period;
    if(slip>c->max_release_us)c->max_release_us=slip;
    if(c->started) {
        int32_t jitter=(int32_t)(now-c->previous)-(int32_t)c->period;
        if(jitter<c->jitter_min)c->jitter_min=jitter;
        if(jitter>c->jitter_max)c->jitter_max=jitter;
        uint32_t absolute=jitter<0 ? (uint32_t)-jitter : (uint32_t)jitter;
        unsigned bin=absolute<10?0:absolute<100?1:absolute<1000?2:absolute<10000?3:absolute<100000?4:5;
        ++c->histogram[bin];
    }
    c->previous=now;c->started=true;
    /* Save this release's lateness separately from the historical maximum. */
    c->release_us=slip;
    return true;
}
void kg_cadence_finish(kg_cadence *c, uint32_t elapsed)
{
    c->late+=(c->release_us+elapsed>c->deadline);
    if(!c->runs || elapsed<c->min_us)c->min_us=elapsed;
    if(elapsed>c->max_us)c->max_us=elapsed;
    c->sum_us+=elapsed;++c->runs;
}
