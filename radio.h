#ifndef FM_RADIO_H
#define FM_RADIO_H

#include "radio_probe.h"
#include "display_layout.h"

#define RADIO_MIN 870
#define RADIO_MAX 1080
#define RADIO_SCALE_LEFT DISPLAY_X(53)
#define RADIO_SCALE_RIGHT DISPLAY_X(275)
#define RADIO_MUTE 1
#define RADIO_SOFTMUTE 2
#define RADIO_DE50 4
#define RADIO_BASS 8
#define RADIO_MONO 16
#define RADIO_AFC 32

enum RadioOperation { RADIO_IDLE, RADIO_TUNING, RADIO_SEEKING };

struct Radio {
    struct RadioPower power;
    unsigned int frequency;
    unsigned int status_a;
    unsigned int status_b;
    char volume;
    char flags;
    char elapsed;
    enum RadioOperation operation;
    bool valid;
    bool frequency_valid;
};

void radio_refresh(struct Radio *radio);
void radio_configure(struct Radio *radio, char volume, char flags, unsigned int frequency);
void radio_audio(struct Radio *radio, char volume, char flags);
void radio_tune(struct Radio *radio, unsigned int frequency);
void radio_seek(struct Radio *radio, bool up);
void radio_cancel(struct Radio *radio);
void radio_poll(struct Radio *radio);
void radio_frequency_text(unsigned int frequency, char *text);
bool radio_parse_frequency(const char *text, unsigned int *frequency);
unsigned int radio_scale_frequency(unsigned int x);
unsigned int radio_scale_position(unsigned int frequency);

#endif
