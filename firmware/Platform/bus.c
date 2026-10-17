#include "board.h"
#include "bus.h"
/* Register adapter for the Wokwi C0 model. HAL still owns peripheral init.
 * TXDR writes are separated because this model exposes TXIS before the prior
 * transmit event has consumed TXDR. RXDR must be consumed immediately. */
static void tx_settle(void)
{
    uint32_t before = SysTick->VAL;
    uint32_t ticks = 0;
    uint32_t period = SysTick->LOAD + 1;
    while (ticks < SystemCoreClock / 4000U) {
        uint32_t value = SysTick->VAL;
        ticks += before >= value ? before - value : before + period - value;
        before = value;
    }
}
static kg_status await_flag(uint32_t flag, uint32_t start)
{
    for (;;) {
        uint32_t status = I2C1->ISR;
        if (status & I2C_ISR_NACKF) return KG_NACK;
        if (status & flag) return KG_OK;
        if ((uint32_t)(HAL_GetTick() - start) >= 5U) return KG_TIMEOUT;
    }
}
static void release_bus(kg_status status)
{
    if (status != KG_OK) {
        I2C1->CR2 |= I2C_CR2_STOP;
        I2C1->CR1 &= ~I2C_CR1_PE;
        I2C1->CR1 |= I2C_CR1_PE;
    }
    I2C1->ICR = I2C_ICR_STOPCF | I2C_ICR_NACKCF;
    I2C1->CR2 = 0;
}
static kg_status transfer(uint8_t address, uint8_t *bytes, size_t count, bool receive)
{
    uint32_t start = HAL_GetTick();
    I2C1->ICR = I2C_ICR_STOPCF | I2C_ICR_NACKCF;
    I2C1->CR2 = ((uint32_t)address << 1) | ((uint32_t)count << 16) |
                I2C_CR2_AUTOEND | (receive ? I2C_CR2_RD_WRN : 0) | I2C_CR2_START;
    kg_status status = KG_OK;
    for (size_t i = 0; i < count; ++i) {
        status = await_flag(receive ? I2C_ISR_RXNE : I2C_ISR_TXIS, start);
        if (status != KG_OK) break;
        if (receive) bytes[i] = (uint8_t)I2C1->RXDR;
        else {
            I2C1->TXDR = bytes[i];
            tx_settle();
        }
    }
    if (status == KG_OK) status = await_flag(I2C_ISR_STOPF, start);
    release_bus(status);
    return status;
}
static kg_status read_register(void *context, uint8_t address, uint8_t reg,
                               uint8_t *bytes, size_t count)
{
    (void)context;
    kg_status status = transfer(address, &reg, 1, false);
    return status == KG_OK ? transfer(address, bytes, count, true) : status;
}
static kg_status write_register(void *context, uint8_t address, uint8_t reg,
                                const uint8_t *bytes, size_t count)
{
    (void)context;
    if (count != 1) return KG_NACK;
    uint8_t packet[] = {reg, bytes[0]};
    return transfer(address, packet, 2, false);
}
kg_bus kg_platform_bus(void)
{
    kg_bus bus = {NULL, read_register, write_register};
    return bus;
}
