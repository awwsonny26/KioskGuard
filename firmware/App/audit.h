#ifndef KG_AUDIT_H
#define KG_AUDIT_H
#include "kiosk.h"
#include <stddef.h>
#define KG_AUDIT_PAGE 2048U
#define KG_AUDIT_RECORD 64U
/* A checkpoint of the terminal's audit counters, not an unlimited event log. */
typedef struct {
    uint32_t near, shocks, moves, lights, errors, services;
    uint32_t last_type, last_ms, last_metric, service, baseline_adc;
} kg_audit_totals;
typedef struct {
    void *context;
    bool (*read)(void *, unsigned offset, void *, size_t);
    bool (*program)(void *, unsigned offset, const uint8_t bytes[8]);
    bool (*erase)(void *, unsigned page);
} kg_audit_io;
typedef struct {
    kg_audit_io io;
    kg_audit_totals totals;
    uint32_t generation, committed, failures, invalid, coalesced;
    unsigned page, slot, cursor;
    uint8_t image[KG_AUDIT_RECORD];
    bool found, dirty, busy, erase_needed, failed;
} kg_audit;
bool kg_audit_mount(kg_audit *, kg_audit_io);
void kg_audit_event(kg_audit *, const kg_event *, uint16_t baseline_adc);
void kg_audit_request(kg_audit *);
/* At most one real erase or aligned doubleword program per invocation. */
bool kg_audit_step(kg_audit *);
uint32_t kg_audit_crc(const uint8_t *, size_t);
#endif
