#ifndef FM_SETTINGS_H
#define FM_SETTINGS_H

#include "radio.h"

#define SETTINGS_SIZE 192

struct Preset {
    unsigned int frequency;
    char name[16];
};

struct Settings {
    unsigned int generation;
    unsigned int frequency;
    char volume;
    char flags;
    struct Preset presets[10];
    char slot;
    char warning;
};

void settings_default(struct Settings *settings);
void settings_encode(const struct Settings *settings, char *bytes);
bool settings_decode(struct Settings *settings, const char *bytes);
char settings_load(struct Settings *settings);
char settings_save(struct Settings *settings);

#endif
