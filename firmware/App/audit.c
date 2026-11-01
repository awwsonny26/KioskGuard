#include "audit.h"
#include <string.h>
#define AUDIT_MAGIC 0x3341474bU
#define COMMIT_A 0x434f4d33U
#define COMMIT_B 0xbcb0b2ccU
static uint32_t word(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24;
}
static void put(uint8_t *p, uint32_t v)
{
    for (unsigned i=0;i<4;++i) p[i]=(uint8_t)(v>>(8*i));
}
uint32_t kg_audit_crc(const uint8_t *p, size_t n)
{
    uint32_t crc=~0U;
    while(n--) {
        crc^=*p++;
        for(unsigned i=0;i<8;++i) crc=(crc>>1)^(0xedb88320U & (0U-(crc&1U)));
    }
    return ~crc;
}
static bool erased(const uint8_t *p)
{
    for(unsigned i=0;i<KG_AUDIT_RECORD;++i) if(p[i]!=255) return false;
    return true;
}
static bool valid(const uint8_t *p)
{
    return word(p)==AUDIT_MAGIC && word(p+56)==COMMIT_A && word(p+60)==COMMIT_B &&
           word(p+52)==kg_audit_crc(p,52);
}
static void decode(kg_audit_totals *t, const uint8_t *p)
{
    t->near=word(p+8);t->shocks=word(p+12);t->moves=word(p+16);
    t->lights=word(p+20);t->errors=word(p+24);t->services=word(p+28);
    t->last_type=word(p+32);t->last_ms=word(p+36);t->last_metric=word(p+40);
    t->service=word(p+44);t->baseline_adc=word(p+48);
}
bool kg_audit_mount(kg_audit *a, kg_audit_io io)
{
    memset(a,0,sizeof(*a));a->io=io;
    uint8_t p[KG_AUDIT_RECORD];
    for(unsigned offset=0;offset<2*KG_AUDIT_PAGE;offset+=KG_AUDIT_RECORD) {
        if(!io.read(io.context,offset,p,sizeof(p))) {a->failed=true;return false;}
        if(erased(p)) continue;
        if(!valid(p)) {++a->invalid;continue;}
        uint32_t generation=word(p+4);
        if(!a->found || (int32_t)(generation-a->generation)>0) {
            a->found=true;a->generation=generation;a->page=offset/KG_AUDIT_PAGE;
            a->slot=(offset%KG_AUDIT_PAGE)/KG_AUDIT_RECORD;
            decode(&a->totals,p);
        }
    }
    a->committed=a->generation;
    /* Never reprogram a torn slot. Scan forward for a completely erased one. */
    unsigned begin=a->found ? a->slot+1 : 0;
    for(a->slot=begin;a->slot<KG_AUDIT_PAGE/KG_AUDIT_RECORD;++a->slot) {
        unsigned offset=a->page*KG_AUDIT_PAGE+a->slot*KG_AUDIT_RECORD;
        if(!io.read(io.context,offset,p,sizeof(p))) {a->failed=true;return false;}
        if(erased(p)) return true;
    }
    a->page^=1U;a->slot=0;a->erase_needed=true;
    return true;
}
void kg_audit_request(kg_audit *a)
{
    if(a->dirty) ++a->coalesced;
    a->dirty=true;
}
void kg_audit_event(kg_audit *a, const kg_event *e, uint16_t baseline_adc)
{
    switch(e->type) {
    case KG_USER_NEAR: ++a->totals.near;break;
    case KG_BODY_SHOCK: ++a->totals.shocks;break;
    case KG_BODY_MOVED: ++a->totals.moves;break;
    case KG_LIGHT_CHANGED: ++a->totals.lights;break;
    case KG_SENSOR_ERROR: case KG_ADC_ERROR: ++a->totals.errors;break;
    case KG_SERVICE_ON: case KG_SERVICE_OFF: ++a->totals.services;break;
    default: return;
    }
    a->totals.last_type=(uint32_t)e->type;a->totals.last_ms=e->time;
    a->totals.last_metric=(uint32_t)e->value;a->totals.service=e->service;
    a->totals.baseline_adc=baseline_adc;kg_audit_request(a);
}
static void encode(kg_audit *a)
{
    uint32_t fields[]={AUDIT_MAGIC,++a->generation,a->totals.near,a->totals.shocks,
        a->totals.moves,a->totals.lights,a->totals.errors,a->totals.services,
        a->totals.last_type,a->totals.last_ms,a->totals.last_metric,
        a->totals.service,a->totals.baseline_adc};
    for(unsigned i=0;i<13;++i) put(a->image+i*4,fields[i]);
    put(a->image+52,kg_audit_crc(a->image,52));
    put(a->image+56,COMMIT_A);put(a->image+60,COMMIT_B);
}
bool kg_audit_step(kg_audit *a)
{
    if(a->failed || (!a->busy && !a->dirty)) return false;
    if(!a->busy) {encode(a);a->dirty=false;a->busy=true;a->cursor=0;}
    bool ok;
    if(a->erase_needed) {
        ok=a->io.erase(a->io.context,a->page);
        if(ok) a->erase_needed=false;
    } else {
        unsigned offset=a->page*KG_AUDIT_PAGE+a->slot*KG_AUDIT_RECORD+a->cursor;
        ok=a->io.program(a->io.context,offset,a->image+a->cursor);
        if(ok) {
            uint8_t check[8];
            ok=a->io.read(a->io.context,offset,check,8) && !memcmp(check,a->image+a->cursor,8);
        }
        if(ok) {
            a->cursor+=8;
            if(a->cursor==KG_AUDIT_RECORD) {
                a->committed=a->generation;a->found=true;a->busy=false;
                if(++a->slot==KG_AUDIT_PAGE/KG_AUDIT_RECORD) {
                    a->slot=0;a->page^=1U;a->erase_needed=true;
                }
            }
        }
    }
    if(!ok) {a->failed=true;++a->failures;}
    return true;
}
