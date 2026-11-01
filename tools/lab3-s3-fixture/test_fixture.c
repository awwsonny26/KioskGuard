/* Host-only mock: tests scheduling; it is not Wokwi or MCU evidence. */
#include <assert.h>
#include <string.h>
#include "workspace/chips/pirfixture.chip.c"
static uint64_t ns; static timer_config_t callbacks[3]; static unsigned n, values, last_value, delay_us; static uart_config_t uartcfg;
pin_t pin_init(const char *name, uint32_t mode){ (void)mode; return !strcmp(name,"OUT")?1:2; }
void pin_write(pin_t p,uint32_t value){ assert(p==1); values++;last_value=value; }
timer_t timer_init(const timer_config_t *c){callbacks[++n]=*c;return n;}
void timer_start(timer_t t,uint32_t us,bool repeat){assert(!repeat);delay_us=us;(void)t;}
double get_sim_nanos_d(void){return (double)ns;}
uart_dev_t uart_init(const uart_config_t *c){uartcfg=*c;return 0;}
static void feed(const char *s){while(*s)uartcfg.rx_data(0,(uint8_t)*s++);}
int main(void){
 chip_init(); assert(n==2 && uartcfg.tx==NO_PIN && uartcfg.baud_rate==115200);
 feed("EXPORT slot=0 ok=1"); assert(delay_us>=20000 && delay_us<=200000 && values==0);
 callbacks[1].callback(0); assert(values==1 && last_value==HIGH && delay_us==5000000);
 feed("EXPORT slot=0 ok=1"); assert(values==1);
 ns=5000000000ULL;callbacks[2].callback(0);assert(values==2 && last_value==LOW);
 delay_us=0; ns+=1199999999ULL; feed("EXPORT slot=0 ");assert(delay_us==0);
 ns+=1;feed("EXPORT slot=0 ");assert(delay_us>=20000 && delay_us<=200000);
 return 0;
}
