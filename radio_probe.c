#include "radio_probe.h"
#include <i2c.h>

#ifndef I2C_SHARED_PORT
#error "The GEOS radio probe requires I2C_SHARED_PORT."
#endif
#ifndef I2C_NO_STDIO
#error "The GEOS radio probe requires I2C_NO_STDIO."
#endif

void radio_format_hex(unsigned int value, char *text)
{
    static const char digits[] = "0123456789ABCDEF";
    for (char remaining = 4; remaining != 0; --remaining)
    {
        text[remaining - 1] = digits[value & 15];
        value >>= 4;
    }
    text[4] = 0;
}

void radio_probe(struct RadioProbe *result)
{
    unsigned char saved_direction = PEEK(DIRREG);
    unsigned char saved_port = PEEK(PORTREG);
    unsigned char bytes[4];

    result->stage = RADIO_PROBE_BUS;
    result->chip_id = 0;
    result->status_a = 0;
    result->status_b = 0;
    result->error = init_i2c_bus();
    if (result->error == APP_RET_SUCCESS)
    {
        result->stage = RADIO_PROBE_ID;
        result->error = i2c_read_register(0x11, 0x00, 2, bytes, false);
        if (result->error == APP_RET_SUCCESS)
        {
            result->chip_id = ((unsigned int)bytes[0] << 8) | bytes[1];
            result->stage = RADIO_PROBE_STATUS;
            result->error = i2c_read_register(0x11, 0x0a, 4, bytes, false);
            if (result->error == APP_RET_SUCCESS)
            {
                result->status_a = ((unsigned int)bytes[0] << 8) | bytes[1];
                result->status_b = ((unsigned int)bytes[2] << 8) | bytes[3];
                result->stage = RADIO_PROBE_COMPLETE;
            }
        }
    }

    /* No GEOS calls/interrupts may intervene between this snapshot and restore. */
    POKE(PORTREG, saved_port);
    POKE(DIRREG, saved_direction);
}

static void power_operation(struct RadioPower *result, bool toggle)
{
    unsigned char saved_direction = PEEK(DIRREG);
    unsigned char saved_port = PEEK(PORTREG);
    unsigned char bytes[2];
    unsigned char enable;

    result->state = RADIO_POWER_UNKNOWN;
    result->stage = RADIO_PROBE_BUS;
    result->chip_id = 0;
    result->control = 0;
    result->error = init_i2c_bus();
    if (result->error != APP_RET_SUCCESS)
        goto restore;

    result->stage = RADIO_PROBE_ID;
    result->error = i2c_read_register(0x11, 0x00, 2, bytes, false);
    if (result->error != APP_RET_SUCCESS)
        goto restore;
    result->chip_id = ((unsigned int)bytes[0] << 8) | bytes[1];
    if (bytes[0] != 0x58)
    {
        result->error = APP_RET_ERR_INVALID_STATE;
        goto restore;
    }

    result->stage = RADIO_POWER_CONTROL;
    result->error = i2c_read_register(0x11, 0x02, 2, bytes, false);
    if (result->error != APP_RET_SUCCESS)
        goto restore;
    result->control = ((unsigned int)bytes[0] << 8) | bytes[1];
    result->state = (bytes[1] & 1) ? RADIO_POWER_ON : RADIO_POWER_OFF;
    if (!toggle)
        goto complete;

    /* Never replay an in-progress SEEK or SOFT_RESET through read-modify-write. */
    if (result->control & 0x0102)
    {
        result->error = APP_RET_ERR_INVALID_STATE;
        goto restore;
    }
    enable = (bytes[1] ^ 1) & 1;
    bytes[1] ^= 1;
    result->state = RADIO_POWER_UNKNOWN;
    result->stage = RADIO_POWER_WRITE;
    result->error = i2c_write_register(0x11, 0x02, 2, bytes);
    if (result->error != APP_RET_SUCCESS)
        goto restore;

    result->stage = RADIO_POWER_VERIFY;
    result->error = i2c_read_register(0x11, 0x02, 2, bytes, false);
    if (result->error != APP_RET_SUCCESS)
        goto restore;
    result->control = ((unsigned int)bytes[0] << 8) | bytes[1];
    if ((bytes[1] & 1) != enable)
    {
        result->error = APP_RET_ERR_INVALID_STATE;
        goto restore;
    }
    result->state = enable ? RADIO_POWER_ON : RADIO_POWER_OFF;
complete:
    result->stage = RADIO_PROBE_COMPLETE;
restore:
    POKE(PORTREG, saved_port);
    POKE(DIRREG, saved_direction);
}

void radio_read_power(struct RadioPower *result)
{
    power_operation(result, false);
}

void radio_toggle_power(struct RadioPower *result)
{
    power_operation(result, true);
}
