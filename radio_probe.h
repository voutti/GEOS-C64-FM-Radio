#ifndef RADIO_PROBE_H
#define RADIO_PROBE_H

#include <common.h>

enum RadioProbeStage {
    RADIO_PROBE_BUS,
    RADIO_PROBE_ID,
    RADIO_PROBE_STATUS,
    RADIO_PROBE_COMPLETE,
    RADIO_POWER_CONTROL,
    RADIO_POWER_WRITE,
    RADIO_POWER_VERIFY
};

enum RadioPowerState {
    RADIO_POWER_UNKNOWN,
    RADIO_POWER_OFF,
    RADIO_POWER_ON
};

struct RadioPower {
    app_ret_code_t error;
    enum RadioProbeStage stage;
    enum RadioPowerState state;
    unsigned int chip_id;
    unsigned int control;
};

struct RadioProbe {
    app_ret_code_t error;
    enum RadioProbeStage stage;
    unsigned int chip_id;
    unsigned int status_a;
    unsigned int status_b;
};

void radio_probe(struct RadioProbe *result);
void radio_format_hex(unsigned int value, char *text);
void radio_read_power(struct RadioPower *result);
void radio_toggle_power(struct RadioPower *result);

#endif
