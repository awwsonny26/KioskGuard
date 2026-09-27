#ifndef KG_KIOSK_H
#define KG_KIOSK_H
#include <stdbool.h>
#include <stdint.h>
#include "../Devices/imu.h"
#define KG_QUEUE_CAPACITY 16U
typedef enum {
    KG_USER_NEAR, KG_BODY_SHOCK, KG_BODY_MOVED, KG_LIGHT_CHANGED,
    KG_BASELINE_READY, KG_BASELINE_PENDING, KG_SENSOR_ERROR,
    KG_SENSOR_RECOVERED, KG_SERVICE_ON, KG_SERVICE_OFF,
    KG_COMMAND_REJECTED, KG_QUEUE_OVERFLOW, KG_ADC_ERROR, KG_ADC_RECOVERED
} kg_event_type;
typedef struct {
    uint32_t number, time;
    kg_event_type type;
    bool service;
    kg_motion motion;
    uint16_t adc;
    int32_t value;
} kg_event;
typedef struct {
    kg_motion motion;
    uint16_t adc;
    bool imu_valid, adc_valid, pir, imu_fresh, adc_fresh;
    kg_status imu_status;
} kg_input;
typedef struct {
    bool service, ready, calibrating, candidate, pir_previous;
    bool moved_pending, moved_latched, light_pending, light_latched;
    bool imu_fault, adc_fault, shock_seen, motion_previous_valid;
    uint32_t stable_since, moved_since, light_since, last_shock;
    uint32_t sequence, lost;
    kg_motion base_motion, anchor_motion, previous_motion;
    uint16_t base_adc, anchor_adc;
    kg_event queue[KG_QUEUE_CAPACITY];
    unsigned head, count;
} kg_kiosk;
void kg_kiosk_init(kg_kiosk *k);
void kg_kiosk_step(kg_kiosk *k, uint32_t now, const kg_input *in);
void kg_request_baseline(kg_kiosk *k, uint32_t now, const kg_input *in);
void kg_command(kg_kiosk *k, uint32_t now, const char *line, const kg_input *in);
bool kg_event_pop(kg_kiosk *k, kg_event *event);
void kg_emit(kg_kiosk *k, uint32_t now, kg_event_type type, const kg_input *in, int32_t value);
const char *kg_event_name(kg_event_type type);
const char *kg_severity(const kg_event *event);
#endif
