#include "settings.h"
#include <geos.h>
#include <string.h>

static const char *const names[2] = {"FM-Radio.cfg0", "FM-Radio.cfg1"};
static char record[SETTINGS_SIZE];
static char verification[SETTINGS_SIZE];
static struct Settings candidate;

static unsigned int get_word(const char *bytes)
{
    return bytes[0] | ((unsigned int)bytes[1] << 8);
}

static void put_word(char *bytes, unsigned int value)
{
    bytes[0] = value;
    bytes[1] = value >> 8;
}

static unsigned int checksum(const char *bytes)
{
    unsigned int crc = 0xffff;
    for (char index = 0; index != SETTINGS_SIZE - 2; ++index)
    {
        crc ^= (unsigned int)bytes[index] << 8;
        for (char bit = 0; bit != 8; ++bit)
            crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : crc << 1;
    }
    return crc;
}

void settings_default(struct Settings *settings)
{
    memset(settings, 0, sizeof(*settings));
    settings->frequency = 880;
    settings->volume = 6;
    settings->flags = RADIO_DE50 | RADIO_AFC;
    settings->slot = 0xff;
}

void settings_encode(const struct Settings *settings, char *bytes)
{
    memcpy(bytes, "FMR1", 4);
    put_word(bytes + 4, settings->generation);
    put_word(bytes + 6, settings->frequency);
    bytes[8] = settings->volume;
    bytes[9] = settings->flags;
    for (char index = 0; index != 10; ++index)
    {
        char offset = 10 + index * 18;
        put_word(bytes + offset, settings->presets[index].frequency);
        memcpy(bytes + offset + 2, settings->presets[index].name, 16);
    }
    put_word(bytes + SETTINGS_SIZE - 2, checksum(bytes));
}

bool settings_decode(struct Settings *settings, const char *bytes)
{
    if (memcmp(bytes, "FMR1", 4) || get_word(bytes + SETTINGS_SIZE - 2) != checksum(bytes))
        return false;
    unsigned int frequency = get_word(bytes + 6);
    if (frequency < RADIO_MIN || frequency > RADIO_MAX || bytes[8] > 15 || (bytes[9] & 0xc0))
        return false;
    for (char index = 0; index != 10; ++index)
    {
        char offset = 10 + index * 18;
        unsigned int preset = get_word(bytes + offset);
        if (preset && (preset < RADIO_MIN || preset > RADIO_MAX))
            return false;
        bool terminated = false;
        for (char letter = 0; letter != 16; ++letter)
        {
            char value = bytes[offset + 2 + letter];
            if (value == 0)
                terminated = true;
            else if (!terminated && (value < 32 || value > 126))
                return false;
        }
        if (!terminated)
            return false;
    }
    settings->generation = get_word(bytes + 4);
    settings->frequency = frequency;
    settings->volume = bytes[8];
    settings->flags = bytes[9];
    for (char index = 0; index != 10; ++index)
    {
        char offset = 10 + index * 18;
        settings->presets[index].frequency = get_word(bytes + offset);
        memcpy(settings->presets[index].name, bytes + offset + 2, 16);
    }
    settings->warning = 0;
    return true;
}

char settings_load(struct Settings *settings)
{
    bool found = false;
    char failure = GEOS_FILE_NOT_FOUND;
    for (char slot = 0; slot != 2; ++slot)
    {
        char error = GeosReadData(names[slot], record, SETTINGS_SIZE);
        if (!error && !settings_decode(&candidate, record))
            error = GEOS_DATA_MISMATCH;
        if (!error)
        {
            unsigned int difference = candidate.generation - settings->generation;
            if (!found || (difference && difference < 0x8000))
            {
                *settings = candidate;
                settings->slot = slot;
            }
            found = true;
        }
        else if (error != GEOS_FILE_NOT_FOUND)
            failure = error;
    }
    if (!found)
        return failure;
    settings->warning = failure == GEOS_FILE_NOT_FOUND ? 0 : failure;
    return 0;
}

char settings_save(struct Settings *settings)
{
    candidate = *settings;
    ++candidate.generation;
    char slot = settings->slot == 0 ? 1 : 0;
    settings_encode(&candidate, record);
    if (!settings_decode(&candidate, record))
        return GEOS_DATA_MISMATCH;
    char error = GeosDeleteFile(names[slot]);
    if (error && error != GEOS_FILE_NOT_FOUND)
        return error;
    error = GeosWriteData(names[slot], record, SETTINGS_SIZE);
    if (error)
        return error;
    error = GeosReadData(names[slot], verification, SETTINGS_SIZE);
    if (error)
        return error;
    if (memcmp(record, verification, SETTINGS_SIZE))
        return GEOS_DATA_MISMATCH;
    settings->generation = candidate.generation;
    settings->slot = slot;
    settings->warning = 0;
    return 0;
}
