#include "wokwi-api.h"
static pin_t output;
static void fire(void *ctx) { (void)ctx; pin_write(output,HIGH); }
void chip_init(void) {
  output=pin_init("OUT",OUTPUT_LOW);
  const timer_config_t config={.callback=fire};
  timer_start(timer_init(&config),5000000,false);
}
