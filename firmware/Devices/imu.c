#include "imu.h"
static int32_t signed_word(const uint8_t *p)
{
    int32_t word = (int32_t)p[0] * 256 + p[1];
    return word >= 32768 ? word - 65536 : word;
}
kg_status kg_imu_start(kg_imu *d)
{
    uint8_t id = 0;
    kg_status status = d->bus.read(d->bus.context, d->address, 0x75, &id, 1);
    if (status != KG_OK) return status;
    if (id != 0x68) return KG_BAD_ID;
    /* Awake, DLPF 94 Hz, +/-4 g, +/-250 deg/s, internal sample 200 Hz. */
    static const uint8_t registers[] = {0x6b, 0x1a, 0x1c, 0x1b, 0x19};
    static const uint8_t values[] = {0x01, 0x02, 0x08, 0x00, 0x04};
    for (unsigned i = 0; i < sizeof(registers); ++i) {
        status = d->bus.write(d->bus.context, d->address, registers[i], &values[i], 1);
        if (status != KG_OK) return status;
    }
    return KG_OK;
}
kg_status kg_imu_read(kg_imu *d, kg_motion *out)
{
    kg_motion sample = {0};
    kg_status status = d->bus.read(d->bus.context, d->address, 0x3b, sample.raw, 14);
    if (status != KG_OK) return status;
    sample.ax = signed_word(sample.raw) * 1000 / 8192;
    sample.ay = signed_word(sample.raw + 2) * 1000 / 8192;
    sample.az = signed_word(sample.raw + 4) * 1000 / 8192;
    sample.temperature = signed_word(sample.raw + 6) * 1000 / 340 + 36530;
    *out = sample; /* A failed read never publishes stale or partial data. */
    return KG_OK;
}
const char *kg_status_name(kg_status status)
{
    static const char *const names[] = {"OK", "NACK", "TIMEOUT", "BAD_ID"};
    return status <= KG_BAD_ID ? names[status] : "UNKNOWN";
}
