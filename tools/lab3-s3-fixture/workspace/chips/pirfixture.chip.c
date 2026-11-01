#include "wokwi-api.h"
#include <stdint.h>
#include <stdbool.h>
/* Passive TX decoder only; output drives the real MCU PIR GPIO input. */
static pin_t out;
static timer_t rise_timer, fall_timer;
static uint32_t rng=35031u, matched;
static uint64_t next_ready_ns;
static bool pending;
static const char key[]="EXPORT slot=0 ";
static void lower(void *unused) { (void)unused; pin_write(out, LOW); next_ready_ns=get_sim_nanos()+1200000000ULL; pending=false; }
static void raise(void *unused) { (void)unused; pin_write(out, HIGH); timer_start(fall_timer,5000000,false); }
static void receive(void *unused, uint8_t c) {
  (void)unused;
  if(c==(uint8_t)key[matched]) matched++; else matched=(c==(uint8_t)key[0]);
  if(!key[matched]) {
    matched=0;
    if(!pending && get_sim_nanos()>=next_ready_ns) {
      pending=true; rng=1664525u*rng+1013904223u;
      timer_start(rise_timer,20000u+(rng%180001u),false);
    }
  }
}
void chip_init(void) {
  out=pin_init("OUT", OUTPUT_LOW);
  pin_t rx=pin_init("RX", INPUT);
  const timer_config_t rise={.callback=raise,.user_data=0}, fall={.callback=lower,.user_data=0};
  rise_timer=timer_init(&rise); fall_timer=timer_init(&fall);
  const uart_config_t u={.rx=rx,.tx=NO_PIN,.baud_rate=115200,.rx_data=receive,.user_data=0};
  uart_init(&u);
}
