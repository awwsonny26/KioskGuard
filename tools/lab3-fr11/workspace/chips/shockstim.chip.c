#include "wokwi-api.h"
#include <stdint.h>
#include <stdbool.h>
/* Narrow test double: MPU ID, setup registers, fixed accel + a timed 2g X step. */
static uint8_t regs[128], addr;
static bool first;
static pin_t stim;
static void fire(void *ctx) { (void)ctx; regs[0x3b]=0x40; regs[0x3c]=0; pin_write(stim,HIGH); }
static bool connect_device(void *ctx,uint32_t address,bool read) { (void)ctx;(void)address;first=!read;return true; }
static bool write_device(void *ctx,uint8_t b) { (void)ctx;if(first){addr=b;first=false;} else {if(addr<128)regs[addr]=b;addr++;} return true; }
static uint8_t read_device(void *ctx) { (void)ctx;return addr<128?regs[addr++]:0; }
void chip_init(void) {
  regs[0x75]=0x68; regs[0x3f]=0x20; /* az=8192 = 1 g in ±4g mode */
  stim=pin_init("STIM",OUTPUT_LOW);
  const i2c_config_t bus={.address=0x68,.scl=pin_init("SCL",INPUT_PULLUP),.sda=pin_init("SDA",INPUT_PULLUP),.connect=connect_device,.read=read_device,.write=write_device};i2c_init(&bus);
  const timer_config_t config={.callback=fire}; timer_start(timer_init(&config),5000000,false);
}
