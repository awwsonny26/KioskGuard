#include "board.h"
#include "bus.h"
static kg_status convert(HAL_StatusTypeDef s)
{
    if (s == HAL_OK) return KG_OK;
    return s == HAL_TIMEOUT ? KG_TIMEOUT : KG_NACK;
}
static kg_status read_register(void *context, uint8_t address, uint8_t reg,
                               uint8_t *bytes, size_t count)
{
    I2C_HandleTypeDef *bus = context;
    return convert(HAL_I2C_Mem_Read(bus, (uint16_t)address << 1, reg,
                   I2C_MEMADD_SIZE_8BIT, bytes, (uint16_t)count, 5));
}
static kg_status write_register(void *context, uint8_t address, uint8_t reg,
                                const uint8_t *bytes, size_t count)
{
    I2C_HandleTypeDef *bus = context;
    return convert(HAL_I2C_Mem_Write(bus, (uint16_t)address << 1, reg,
                   I2C_MEMADD_SIZE_8BIT, (uint8_t *)bytes, (uint16_t)count, 5));
}
kg_bus kg_platform_bus(void)
{
    kg_bus bus = {&hi2c1, read_register, write_register};
    return bus;
}
