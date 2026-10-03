#include "kiosk.h"
#include <string.h>
static int64_t length2(const kg_motion *v)
{
    return (int64_t)v->ax*v->ax + (int64_t)v->ay*v->ay + (int64_t)v->az*v->az;
}
static int64_t distance2(const kg_motion *a, const kg_motion *b)
{
    int64_t x=(int64_t)a->ax-b->ax, y=(int64_t)a->ay-b->ay, z=(int64_t)a->az-b->az;
    return x*x+y*y+z*z;
}
static bool quasistatic(const kg_motion *v)
{
    int64_t n = length2(v);
    return n >= 810000 && n <= 1210000; /* 0.9..1.1 g */
}
static unsigned adc_delta(uint16_t a, uint16_t b)
{
    return a > b ? a-b : b-a;
}
void kg_emit(kg_kiosk *k, uint32_t now, kg_event_type type, const kg_input *in, int32_t value)
{
    kg_event e = {0};
    e.time = now;
    e.type = type;
    e.service = k->service;
    e.motion = in->motion;
    e.adc = in->adc;
    e.value = value;
    if (k->count == KG_QUEUE_CAPACITY) {
        ++k->lost;
        k->overflow = e;
        return;
    }
    e.number = ++k->sequence;
    k->queue[(k->head + k->count) % KG_QUEUE_CAPACITY] = e;
    ++k->count;
}
bool kg_event_pop(kg_kiosk *k, kg_event *e)
{
    if (k->count) {
        *e = k->queue[k->head];
        k->head = (k->head+1) % KG_QUEUE_CAPACITY;
        --k->count;
        return true;
    }
    if (k->lost) {
        *e = k->overflow;
        e->number = ++k->sequence;
        e->type = KG_QUEUE_OVERFLOW;
        e->value = (int32_t)k->lost;
        k->lost = 0;
        return true;
    }
    return false;
}
void kg_kiosk_init(kg_kiosk *k)
{
    memset(k, 0, sizeof(*k));
    k->calibrating = true;
}
void kg_request_baseline(kg_kiosk *k, uint32_t now, const kg_input *in)
{
    k->ready = false;
    k->calibrating = true;
    k->candidate = false;
    k->moved_pending = k->light_pending = false;
    k->moved_latched = k->light_latched = false;
    k->motion_previous_valid = false;
    k->shock_seen = false;
    kg_emit(k, now, KG_BASELINE_PENDING, in, 0);
}
static void calibrate(kg_kiosk *k, uint32_t now, const kg_input *in)
{
    if (!in->imu_valid || !in->adc_valid || !quasistatic(&in->motion)) {
        k->candidate = false;
        return;
    }
    if (!in->imu_fresh && !in->adc_fresh) return;
    if (!k->candidate || distance2(&in->motion, &k->anchor_motion) > 2500 ||
        adc_delta(in->adc, k->anchor_adc) > 82) {
        k->anchor_motion = in->motion;
        k->anchor_adc = in->adc;
        k->stable_since = now;
        k->candidate = true;
    }
    if ((uint32_t)(now-k->stable_since) >= 2000) {
        k->base_motion = k->anchor_motion;
        k->base_adc = k->anchor_adc;
        k->ready = true;
        k->calibrating = false;
        k->previous_motion = in->motion;
        k->motion_previous_valid = true;
        kg_emit(k, now, KG_BASELINE_READY, in, k->base_adc);
    }
}
void kg_kiosk_step(kg_kiosk *k, uint32_t now, const kg_input *in)
{
    if (in->pir && !k->pir_previous) kg_emit(k, now, KG_USER_NEAR, in, 0);
    k->pir_previous = in->pir;
    if (!in->imu_valid && !k->imu_fault)
        kg_emit(k, now, KG_SENSOR_ERROR, in, in->imu_status);
    if (in->imu_valid && k->imu_fault)
        kg_emit(k, now, KG_SENSOR_RECOVERED, in, 0);
    k->imu_fault = !in->imu_valid;
    if (!in->adc_valid && !k->adc_fault) kg_emit(k, now, KG_ADC_ERROR, in, 0);
    if (in->adc_valid && k->adc_fault) kg_emit(k, now, KG_ADC_RECOVERED, in, 0);
    k->adc_fault = !in->adc_valid;
    if (k->calibrating) calibrate(k, now, in);
}
const char *kg_event_name(kg_event_type type)
{
    static const char *const names[] = {
        "USER_NEAR", "BODY_SHOCK", "BODY_MOVED", "LIGHT_CHANGED",
        "BASELINE_READY", "BASELINE_PENDING", "SENSOR_ERROR", "SENSOR_RECOVERED",
        "SERVICE_ON", "SERVICE_OFF", "COMMAND_REJECTED", "QUEUE_OVERFLOW",
        "ADC_ERROR", "ADC_RECOVERED"
    };
    return (unsigned)type < sizeof(names)/sizeof(names[0]) ? names[type] : "UNKNOWN";
}
const char *kg_severity(const kg_event *e)
{
    if (e->type == KG_SENSOR_ERROR || e->type == KG_ADC_ERROR || e->type == KG_QUEUE_OVERFLOW)
        return "ERROR";
    if (e->type >= KG_BODY_SHOCK && e->type <= KG_LIGHT_CHANGED)
        return e->service ? "INFO" : "WARN";
    return "INFO";
}
