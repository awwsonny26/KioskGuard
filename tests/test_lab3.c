#include "../firmware/App/audit.h"
#include "../firmware/App/cadence.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
typedef struct {uint8_t bytes[4096];int cut;unsigned operations;bool fail_read;} flash_model;
static bool rd(void *ctx,unsigned offset,void *out,size_t n)
{
    flash_model *f=ctx;assert(offset+n<=sizeof(f->bytes));
    if(f->fail_read)return false;
    memcpy(out,f->bytes+offset,n);return true;
}
static bool pg(void *ctx,unsigned offset,const uint8_t bytes[8])
{
    flash_model *f=ctx;assert(offset%8==0 && offset+8<=4096);++f->operations;
    for(unsigned i=0;i<8;++i) {
        if(f->cut==0)return false;
        if(f->cut>0)--f->cut;
        assert(f->bytes[offset+i]==255);f->bytes[offset+i]&=bytes[i];
    }
    return true;
}
static bool er(void *ctx,unsigned page)
{
    flash_model *f=ctx;assert(page<2);++f->operations;
    for(unsigned i=0;i<2048;++i) {
        if(f->cut==0)return false;
        if(f->cut>0)--f->cut;
        f->bytes[page*2048+i]=255;
    }
    return true;
}
static kg_audit_io io(flash_model *f)
{kg_audit_io x={f,rd,pg,er};return x;}
static void fresh(flash_model *f)
{memset(f,0,sizeof(*f));memset(f->bytes,255,4096);f->cut=-1;}
static void flush(kg_audit *a)
{
    unsigned steps=0;
    while(kg_audit_step(a))assert(++steps<20);
    assert(!a->failed && !a->busy && !a->dirty);
}
static void event(kg_audit *a,kg_event_type type,bool service)
{
    kg_event e={0};e.type=type;e.service=service;e.time=12345;e.value=-17;
    kg_audit_event(a,&e,1001);
}
static void separation(void)
{
    flash_model f;fresh(&f);kg_audit a,b;assert(kg_audit_mount(&a,io(&f)));
    event(&a,KG_USER_NEAR,false);event(&a,KG_BODY_SHOCK,false);
    event(&a,KG_BODY_MOVED,true);event(&a,KG_LIGHT_CHANGED,true);
    event(&a,KG_SENSOR_ERROR,true);event(&a,KG_SERVICE_ON,true);flush(&a);
    assert(kg_audit_mount(&b,io(&f)) && b.found && b.committed==1);
    assert(b.totals.near==1 && b.totals.shocks==1 && b.totals.moves==1);
    assert(b.totals.services==1 && b.totals.service==1 && b.totals.baseline_adc==1001);
    assert(b.totals.last_metric==(uint32_t)-17);
    event(&a,KG_COMMAND_REJECTED,false);assert(!a.dirty);
    puts("PASS audit: separate user/mechanical/light/service counters, coalesced checkpoint, reboot");
}
static void cut_records(void)
{
    flash_model saved;fresh(&saved);kg_audit a;assert(kg_audit_mount(&a,io(&saved)));
    event(&a,KG_USER_NEAR,false);flush(&a);
    for(int cut=0;cut<=64;++cut) {
        flash_model f=saved;kg_audit writer,reboot;
        assert(kg_audit_mount(&writer,io(&f)));f.cut=cut;
        event(&writer,KG_BODY_SHOCK,false);while(kg_audit_step(&writer)){}
        f.cut=-1;assert(kg_audit_mount(&reboot,io(&f)) && reboot.found);
        assert(reboot.totals.near==1);
        assert(reboot.totals.shocks==(unsigned)(cut==64));
        event(&reboot,KG_LIGHT_CHANGED,true);flush(&reboot);
        kg_audit final;assert(kg_audit_mount(&final,io(&f)));
        assert(final.totals.lights==1 && final.totals.shocks==(unsigned)(cut==64));
    }
    puts("PASS audit: 65 byte cut positions, previous checkpoint retained, torn slots skipped");
}
static void rotation(void)
{
    flash_model saved;fresh(&saved);kg_audit a;assert(kg_audit_mount(&a,io(&saved)));
    for(unsigned i=0;i<64;++i){event(&a,KG_BODY_SHOCK,false);flush(&a);}
    assert(a.totals.shocks==64 && a.erase_needed);
    for(int cut=0;cut<=2112;++cut) {
        flash_model f=saved;kg_audit writer,reboot;assert(kg_audit_mount(&writer,io(&f)));
        assert(writer.erase_needed);f.cut=cut;event(&writer,KG_SERVICE_ON,true);
        while(kg_audit_step(&writer)){}
        f.cut=-1;assert(kg_audit_mount(&reboot,io(&f)) && reboot.found);
        assert(reboot.totals.shocks==64 && reboot.totals.services==(unsigned)(cut==2112));
    }
    puts("PASS audit: 2113 power-cut positions across page rotation erase + record commit");
}
static void corruption(void)
{
    flash_model saved;fresh(&saved);kg_audit a;assert(kg_audit_mount(&a,io(&saved)));
    event(&a,KG_USER_NEAR,false);flush(&a);event(&a,KG_BODY_SHOCK,false);flush(&a);
    for(unsigned bit=0;bit<512;++bit) {
        flash_model f=saved;f.bytes[64+bit/8]^=(uint8_t)(1U<<(bit%8));
        kg_audit b;assert(kg_audit_mount(&b,io(&f)) && b.committed==1 && b.invalid==1);
    }
    saved.fail_read=true;kg_audit b;assert(!kg_audit_mount(&b,io(&saved)) && b.failed);
    puts("PASS audit: all 512 single-bit corruptions rejected, mount read failure explicit");
}
static void snapshot(void)
{
    flash_model f;fresh(&f);kg_audit a,b;assert(kg_audit_mount(&a,io(&f)));
    event(&a,KG_USER_NEAR,false);assert(kg_audit_step(&a));
    event(&a,KG_BODY_SHOCK,false);
    for(unsigned i=0;i<7;++i)assert(kg_audit_step(&a));
    assert(kg_audit_mount(&b,io(&f)) && b.totals.near==1 && b.totals.shocks==0);
    flush(&a);assert(kg_audit_mount(&b,io(&f)) && b.totals.shocks==1 && b.committed==2);
    puts("PASS audit: event during program preserves immutable record and schedules next checkpoint");
}
static void scheduling(void)
{
    kg_cadence c={.period=20000,.deadline=20000,.next=UINT32_MAX-9999};
    uint32_t now=UINT32_MAX-9999;
    assert(kg_cadence_due(&c,now));kg_cadence_finish(&c,100);
    now+=20000;assert(kg_cadence_due(&c,now));kg_cadence_finish(&c,200);
    assert(c.runs==2 && c.skipped==0 && c.jitter_max==0);
    now+=65000;assert(kg_cadence_due(&c,now));kg_cadence_finish(&c,16000);
    assert(c.skipped==2 && c.jitter_max==45000 && c.late==1);
    assert(c.next==now+15000 && c.sum_us==16300 && c.histogram[4]==1);
    assert(!kg_cadence_due(&c,now+14999));
    /* Logical time only: verifies wrap arithmetic, not an hour of MCU execution. */
    for(unsigned i=0;i<200000;++i) {
        now=c.next;assert(kg_cadence_due(&c,now));kg_cadence_finish(&c,70);
    }
    assert(c.runs==200003 && c.skipped==2);
    puts("PASS cadence: wrap, 200003 releases, no catch-up burst, skipped slots and completion deadline");
}
int main(void)
{
    separation();cut_records();rotation();corruption();snapshot();scheduling();
    puts("PASS lab3 host model only; no MCU timing or physical power-loss claim");
}
