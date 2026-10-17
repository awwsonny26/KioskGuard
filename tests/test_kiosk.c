#include "../firmware/App/kiosk.h"
#include "../firmware/App/line.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include "fixtures/mpu_rest.h"
static unsigned groups;
static void pass(const char *name) { ++groups; printf("PASS %02u %s\n", groups, name); }
static kg_input rest(void)
{
    kg_input in={0};
    in.motion.az=1000;in.adc=1001;
    in.imu_valid=in.adc_valid=in.imu_fresh=in.adc_fresh=true;
    return in;
}
static void drain(kg_kiosk *k) { kg_event e;while(kg_event_pop(k,&e)) {} }
static kg_event pop(kg_kiosk *k,kg_event_type type)
{
    kg_event e;assert(kg_event_pop(k,&e));assert(e.type==type);return e;
}
static kg_kiosk ready(kg_input *in,uint32_t start)
{
    kg_kiosk k;kg_kiosk_init(&k);kg_kiosk_step(&k,start,in);
    kg_kiosk_step(&k,start+1999,in);assert(!k.ready);
    kg_kiosk_step(&k,start+2000,in);assert(k.ready);
    pop(&k,KG_BASELINE_READY);return k;
}
typedef struct { unsigned call,fail;kg_status failure;uint8_t id,frame[14]; } fake_bus;
static kg_status fake_read(void *ctx,uint8_t addr,uint8_t reg,uint8_t *data,size_t n)
{
    fake_bus *f=ctx;assert(addr==0x68);++f->call;
    if(f->call==f->fail)return f->failure;
    if(reg==0x75){assert(n==1);*data=f->id;}
    else {assert(reg==0x3b && n==14);memcpy(data,f->frame,14);}
    return KG_OK;
}
static kg_status fake_write(void *ctx,uint8_t addr,uint8_t reg,const uint8_t *data,size_t n)
{
    fake_bus *f=ctx;
    static const uint8_t r[]={0x6b,0x1a,0x1c,0x1b,0x19};
    static const uint8_t v[]={1,2,8,0,4};
    assert(addr==0x68 && n==1);
    unsigned i=f->call-1;assert(i<5);assert(reg==r[i] && *data==v[i]);
    ++f->call;
    return f->call==f->fail ? f->failure : KG_OK;
}
static kg_imu device(fake_bus *f)
{
    kg_bus bus={f,fake_read,fake_write};kg_imu d={bus,0x68};return d;
}
static void test_driver(void)
{
    fake_bus f={0};f.id=0x68;kg_imu d=device(&f);
    assert(kg_imu_start(&d)==KG_OK && f.call==6);
    for(unsigned failure=KG_NACK;failure<=KG_TIMEOUT;++failure)
        for(unsigned step=1;step<=6;++step){
            memset(&f,0,sizeof(f));f.id=0x68;f.fail=step;f.failure=(kg_status)failure;
            assert(kg_imu_start(&d)==(kg_status)failure && f.call==step);
        }
    memset(&f,0,sizeof(f));f.id=0x69;
    assert(kg_imu_start(&d)==KG_BAD_ID && f.call==1);
    assert(strcmp(kg_status_name(KG_BAD_ID),"BAD_ID")==0);
    assert(strcmp(kg_status_name((kg_status)99),"UNKNOWN")==0);
    pass("MPU ID, initialization order, NACK and timeout at every transaction");
}
static void test_units(void)
{
    fake_bus f={0};kg_imu d=device(&f);kg_motion out={0};
    memcpy(f.frame,kg_captured_rest,14);
    assert(kg_imu_read(&d,&out)==KG_OK);
    assert(out.ax==0 && out.ay==0 && out.az==1000 && out.temperature==24001);
    assert(memcmp(out.raw,kg_captured_rest,14)==0);
    memset(f.frame,0,14);f.frame[0]=0x80;f.frame[2]=0x7f;f.frame[3]=0xff;
    assert(kg_imu_read(&d,&out)==KG_OK && out.ax==-4000 && out.ay==3999);
    kg_motion prior=out;f.fail=f.call+1;f.failure=KG_TIMEOUT;
    assert(kg_imu_read(&d,&out)==KG_TIMEOUT && memcmp(&out,&prior,sizeof(out))==0);
    pass("signed units, captured +/-4g frame, failed read leaves sample intact");
}
static void test_baseline(void)
{
    kg_input in=rest();kg_kiosk k;kg_kiosk_init(&k);
    kg_kiosk_step(&k,0,&in);in.motion.ax=100;
    kg_kiosk_step(&k,1900,&in);kg_kiosk_step(&k,2000,&in);assert(!k.ready);
    kg_kiosk_step(&k,3900,&in);assert(k.ready);pop(&k,KG_BASELINE_READY);
    assert(k.base_motion.ax==100 && k.base_adc==1001);
    kg_request_baseline(&k,4000,&in);pop(&k,KG_BASELINE_PENDING);
    in.motion.ax=2000;in.adc=3000;
    kg_kiosk_step(&k,4000,&in);kg_kiosk_step(&k,9000,&in);
    assert(!k.ready && !k.count);
    in=rest();kg_kiosk_step(&k,9020,&in);kg_kiosk_step(&k,11020,&in);
    assert(k.ready);pop(&k,KG_BASELINE_READY);
    kg_request_baseline(&k,12000,&in);drain(&k);
    in.adc_fresh=in.imu_fresh=false;kg_kiosk_step(&k,15000,&in);assert(!k.ready);
    in.adc_fresh=true;kg_kiosk_step(&k,15000,&in);
    in.adc+=83;kg_kiosk_step(&k,16990,&in);kg_kiosk_step(&k,17000,&in);assert(!k.ready);
    kg_kiosk_step(&k,18990,&in);assert(k.ready);
    pass("baseline 2 seconds, motion/light restart stability, no warnings before ready");
}
static void test_pir(void)
{
    kg_input in=rest();kg_kiosk k=ready(&in,0);in.pir=true;
    kg_kiosk_step(&k,2020,&in);kg_event e=pop(&k,KG_USER_NEAR);
    assert(!strcmp(kg_severity(&e),"INFO"));
    for(uint32_t t=2040;t<=7020;t+=20)kg_kiosk_step(&k,t,&in);
    assert(!k.count);in.pir=false;kg_kiosk_step(&k,7040,&in);
    in.pir=true;kg_kiosk_step(&k,7060,&in);pop(&k,KG_USER_NEAR);
    pass("PIR HIGH 5 seconds gives one INFO; LOW then HIGH rearms");
}
static void test_shock(void)
{
    kg_input in=rest();kg_kiosk k=ready(&in,0);
    in.motion.ax=1000;kg_kiosk_step(&k,2020,&in);assert(!k.count);
    in.motion.ax=0;kg_kiosk_step(&k,2040,&in);assert(!k.count);
    in.motion.ax=2000;kg_kiosk_step(&k,2060,&in);
    kg_event e=pop(&k,KG_BODY_SHOCK);assert(e.value==4000000);
    assert(!strcmp(kg_severity(&e),"WARN"));
    in.motion.ax=0;kg_kiosk_step(&k,2080,&in);assert(!k.count);
    in.motion.ax=2000;kg_kiosk_step(&k,2540,&in);assert(!k.count);
    in.motion.ax=0;kg_kiosk_step(&k,2560,&in);pop(&k,KG_BODY_SHOCK);
    assert(!k.moved_pending && k.base_motion.ax==0);
    pass("vector shock >1g, exact threshold, 500ms cooldown, dynamic acceleration gate");
}
static void test_moved(void)
{
    kg_input in=rest();kg_kiosk k=ready(&in,0);
    in.motion.ax=707;in.motion.az=707;
    kg_kiosk_step(&k,2020,&in);kg_kiosk_step(&k,3019,&in);assert(!k.count);
    kg_kiosk_step(&k,3020,&in);pop(&k,KG_BODY_MOVED);
    kg_kiosk_step(&k,5020,&in);assert(!k.count && k.base_motion.az==1000);
    in.motion.ax=309;in.motion.az=951;kg_kiosk_step(&k,5040,&in);
    assert(k.moved_latched);
    in=rest();kg_kiosk_step(&k,5060,&in);assert(!k.moved_latched);
    in.motion.ax=707;in.motion.az=707;kg_kiosk_step(&k,5080,&in);
    kg_kiosk_step(&k,6080,&in);pop(&k,KG_BODY_MOVED);
    in=rest();kg_kiosk_step(&k,6100,&in);
    in.motion.az=-1000;kg_kiosk_step(&k,6600,&in);pop(&k,KG_BODY_SHOCK);
    kg_kiosk_step(&k,7600,&in);pop(&k,KG_BODY_MOVED);
    pass("45-degree movement 1000ms, 15-degree rearm, 180-degree orientation");
}
static void test_confirmation_reset(void)
{
    kg_input in=rest();kg_kiosk k=ready(&in,0);
    in.motion.ax=707;in.motion.az=707;kg_kiosk_step(&k,2020,&in);
    in.motion.ax=2000;in.motion.az=1000;kg_kiosk_step(&k,2900,&in);drain(&k);
    assert(!k.moved_pending);in.motion.ax=707;in.motion.az=707;
    kg_kiosk_step(&k,2920,&in);drain(&k);kg_kiosk_step(&k,3919,&in);assert(!k.count);
    kg_kiosk_step(&k,3920,&in);pop(&k,KG_BODY_MOVED);
    pass("dynamic sample interrupts orientation confirmation");
}
static void test_light(void)
{
    kg_input in=rest();kg_kiosk k=ready(&in,0);
    in.adc=1615;kg_kiosk_step(&k,2050,&in);kg_kiosk_step(&k,2350,&in);assert(!k.count);
    in.adc=1616;kg_kiosk_step(&k,2400,&in);kg_kiosk_step(&k,2699,&in);assert(!k.count);
    kg_kiosk_step(&k,2700,&in);kg_event e=pop(&k,KG_LIGHT_CHANGED);assert(e.value==615);
    in.adc=1411;kg_kiosk_step(&k,2750,&in);assert(k.light_latched);
    in.adc=1410;kg_kiosk_step(&k,2800,&in);assert(!k.light_latched);
    in.adc=1616;kg_kiosk_step(&k,2850,&in);kg_kiosk_step(&k,3150,&in);
    pop(&k,KG_LIGHT_CHANGED);assert(k.base_adc==1001);
    in.adc=0;kg_kiosk_step(&k,3200,&in);assert(k.light_latched);
    pass("15-percent ADC threshold, 300ms confirmation, 10-percent hysteresis");
}
static void test_short_light(void)
{
    kg_input in=rest();kg_kiosk k=ready(&in,0);
    in.adc=3000;kg_kiosk_step(&k,2050,&in);
    in.adc=1001;kg_kiosk_step(&k,2100,&in);kg_kiosk_step(&k,2500,&in);
    assert(!k.count);
    in.adc=3000;kg_kiosk_step(&k,2550,&in);in.adc_fresh=false;
    kg_kiosk_step(&k,2850,&in);assert(!k.count);
    in.adc_fresh=true;in.adc_valid=false;kg_kiosk_step(&k,2870,&in);
    pop(&k,KG_ADC_ERROR);assert(!k.light_pending);
    in.adc_valid=true;kg_kiosk_step(&k,2900,&in);pop(&k,KG_ADC_RECOVERED);
    kg_kiosk_step(&k,3200,&in);pop(&k,KG_LIGHT_CHANGED);
    pass("50ms light pulse rejected, stale samples and ADC fault reset confirmation");
}
static void test_commands_and_queue(void)
{
    kg_input in=rest();kg_kiosk k=ready(&in,0);
    kg_command(&k,2010,"service on",&in);kg_event a=pop(&k,KG_SERVICE_ON);assert(a.service);
    in.pir=true;in.motion.ax=2000;kg_kiosk_step(&k,2020,&in);
    kg_event p=pop(&k,KG_USER_NEAR),s=pop(&k,KG_BODY_SHOCK);
    assert(p.number+1==s.number && s.time==2020 && s.motion.ax==2000);
    assert(!strcmp(kg_severity(&p),"INFO") && !strcmp(kg_severity(&s),"INFO"));
    kg_command(&k,2030,"service off",&in);pop(&k,KG_SERVICE_OFF);
    assert(s.service); /* Snapshot keeps the mode at event time. */
    kg_command(&k,2040,"service ON",&in);pop(&k,KG_COMMAND_REJECTED);assert(!k.service);
    kg_command(&k,2050,"baseline",&in);pop(&k,KG_BASELINE_PENDING);assert(!k.ready);
    assert(!strcmp(kg_event_name((kg_event_type)100),"UNKNOWN"));
    pass("UART modes, rejection, baseline, simultaneous PIR/shock and immutable records");
}
static void test_faults(void)
{
    kg_input in=rest();kg_kiosk k=ready(&in,0);
    in.imu_valid=false;in.imu_status=KG_NACK;
    kg_kiosk_step(&k,2020,&in);kg_event e=pop(&k,KG_SENSOR_ERROR);
    assert(e.value==KG_NACK && !strcmp(kg_severity(&e),"ERROR"));
    in.pir=true;in.adc=3000;kg_kiosk_step(&k,2040,&in);pop(&k,KG_USER_NEAR);
    kg_kiosk_step(&k,2340,&in);pop(&k,KG_LIGHT_CHANGED);assert(!k.count);
    kg_kiosk_step(&k,3020,&in);assert(!k.count);
    in.imu_valid=true;in.motion.ax=2000;kg_kiosk_step(&k,3040,&in);
    pop(&k,KG_SENSOR_RECOVERED);assert(!k.count);
    pass("one NACK error, PIR/ADC independent, recovery without false shock");
}
static void test_wrap_overflow(void)
{
    kg_input in=rest();kg_kiosk k=ready(&in,UINT32_MAX-1000);
    assert(k.ready);
    for(unsigned i=0;i<20;++i)kg_emit(&k,i,KG_USER_NEAR,&in,0);
    assert(k.count==16 && k.lost==4);
    uint32_t seq=k.sequence-16;
    for(unsigned i=0;i<16;++i){kg_event e=pop(&k,KG_USER_NEAR);assert(e.number==++seq);}
    kg_event e=pop(&k,KG_QUEUE_OVERFLOW);assert(e.value==4 && e.number==seq+1);
    assert(!strcmp(kg_severity(&e),"ERROR") && !k.lost);
    kg_kiosk_init(&k);assert(k.sequence==0 && !k.count && !k.service);
    pass("32-bit time rollover, queue capacity/loss report, reset counters");
}
static void test_line_button(void)
{
    kg_line l={0};
    const char *s="service on";for(unsigned i=0;s[i];++i)assert(kg_line_feed(&l,s[i])==KG_LINE_WAIT);
    assert(kg_line_feed(&l,'\r')==KG_LINE_READY && !strcmp(l.text,s));
    assert(kg_line_feed(&l,'\n')==KG_LINE_WAIT);
    for(unsigned i=0;i<70;++i)kg_line_feed(&l,'a');
    assert(kg_line_feed(&l,'\n')==KG_LINE_REJECTED);
    kg_line_feed(&l,1);assert(kg_line_feed(&l,'\n')==KG_LINE_REJECTED);
    kg_line_feed(&l,255);assert(kg_line_feed(&l,'\n')==KG_LINE_REJECTED);
    kg_button b={0};
    assert(!kg_button_press(&b,true,0));assert(!kg_button_press(&b,false,10));
    assert(!kg_button_press(&b,true,20));assert(!kg_button_press(&b,true,54));
    assert(kg_button_press(&b,true,55));assert(!kg_button_press(&b,true,1000));
    assert(!kg_button_press(&b,false,1010));assert(!kg_button_press(&b,false,1045));
    assert(!kg_button_press(&b,true,1050));assert(kg_button_press(&b,true,1085));
    pass("CRLF, overlong/binary commands, 35ms button debounce and hold");
}
int main(void)
{
    test_driver();test_units();test_baseline();test_pir();test_shock();
    test_moved();test_confirmation_reset();test_light();test_short_light();
    test_commands_and_queue();test_faults();test_wrap_overflow();test_line_button();
    printf("PASS: %u groups, portable App/Devices only\n",groups);return 0;
}
