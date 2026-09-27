#ifndef KG_IMU_H
#define KG_IMU_H
#include <stddef.h>
#include <stdint.h>
typedef enum { KG_OK, KG_NACK, KG_TIMEOUT, KG_BAD_ID } kg_status;
typedef struct {
    void *context;
    kg_status (*read)(void *, uint8_t, uint8_t, uint8_t *, size_t);
    kg_status (*write)(void *, uint8_t, uint8_t, const uint8_t *, size_t);
} kg_bus;
typedef struct {
    int32_t ax, ay, az; /* milli-g */
    int32_t temperature; /* milli-Celsius */
    uint8_t raw[14];
} kg_motion;
typedef struct { kg_bus bus; uint8_t address; } kg_imu;
kg_status kg_imu_start(kg_imu *device);
kg_status kg_imu_read(kg_imu *device, kg_motion *sample);
const char *kg_status_name(kg_status status);
#endif
