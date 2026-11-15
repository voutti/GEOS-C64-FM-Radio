#include "radio.h"
#include <i2c.h>

static app_ret_code_t read_word(char reg, unsigned int *value)
{
    char bytes[2];
    app_ret_code_t error = i2c_read_register(0x11, reg, 2, bytes, false);
    if (error == APP_RET_SUCCESS)
        *value = ((unsigned int)bytes[0] << 8) | bytes[1];
    return error;
}

static app_ret_code_t update_word(char reg, unsigned int mask, unsigned int value)
{
    unsigned int previous;
    app_ret_code_t error = read_word(reg, &previous);
    if (error != APP_RET_SUCCESS)
        return error;
    unsigned int updated = (previous & ~mask) | (value & mask);
    char bytes[2] = {updated >> 8, updated};
    error = i2c_write_register(0x11, reg, 2, bytes);
    if (error != APP_RET_SUCCESS)
        return error;
    error = read_word(reg, &previous);
    unsigned int verify_mask = reg == 2 ? mask & ~0x0100 : mask;
    if (error == APP_RET_SUCCESS && (previous & verify_mask) != (updated & verify_mask))
        error = APP_RET_ERR_INVALID_STATE;
    return error;
}

static bool ready(struct Radio *radio, bool allow_busy)
{
    radio_read_power(&radio->power);
    if (radio->power.error == APP_RET_SUCCESS &&
        (radio->power.state != RADIO_POWER_ON ||
         (!allow_busy && (radio->operation != RADIO_IDLE || (radio->power.control & 0x0102)))))
        radio->power.error = APP_RET_ERR_INVALID_STATE;
    if (radio->power.error == APP_RET_SUCCESS && !allow_busy)
    {
        char saved_direction = PEEK(DIRREG), saved_port = PEEK(PORTREG);
        unsigned int channel;
        radio->power.error = read_word(3, &channel);
        POKE(PORTREG, saved_port);
        POKE(DIRREG, saved_direction);
        if (!radio->power.error && (channel & 0x10))
            radio->power.error = APP_RET_ERR_INVALID_STATE;
    }
    if (radio->power.error != APP_RET_SUCCESS)
    {
        radio->valid = false;
        return false;
    }
    return true;
}

void radio_refresh(struct Radio *radio)
{
    radio->valid = false;
    radio->frequency_valid = false;
    radio_read_power(&radio->power);
    if (radio->power.error != APP_RET_SUCCESS)
        return;
    char saved_direction = PEEK(DIRREG), saved_port = PEEK(PORTREG);
    unsigned int channel, audio, volume, mode;
    radio->power.stage = RADIO_PROBE_STATUS;
    app_ret_code_t error = read_word(3, &channel);
    if (error == APP_RET_SUCCESS) error = read_word(4, &audio);
    if (error == APP_RET_SUCCESS) error = read_word(5, &volume);
    if (error == APP_RET_SUCCESS) error = read_word(7, &mode);
    if (error == APP_RET_SUCCESS) error = read_word(0x0a, &radio->status_a);
    if (error == APP_RET_SUCCESS) error = read_word(0x0b, &radio->status_b);
    POKE(PORTREG, saved_port);
    POKE(DIRREG, saved_direction);
    radio->power.error = error;
    if (error != APP_RET_SUCCESS)
        return;
    radio->volume = volume & 15;
    radio->flags = 0;
    if (!(radio->power.control & 0x4000)) radio->flags |= RADIO_MUTE;
    if (radio->power.control & 0x2000) radio->flags |= RADIO_MONO;
    if (radio->power.control & 0x1000) radio->flags |= RADIO_BASS;
    if (audio & 0x0800) radio->flags |= RADIO_DE50;
    if (audio & 0x0200) radio->flags |= RADIO_SOFTMUTE;
    if (!(audio & 0x0100)) radio->flags |= RADIO_AFC;
    if (!(channel & 0x2f) && !(mode & 1))
    {
        unsigned int frequency = RADIO_MIN + (radio->status_a & 0x03ff);
        if (frequency <= RADIO_MAX)
        {
            radio->frequency = frequency;
            radio->frequency_valid = radio->power.state == RADIO_POWER_ON &&
                !(radio->power.control & 0x0102) && !(channel & 0x10);
        }
    }
    radio->valid = true;
    radio->power.stage = RADIO_PROBE_COMPLETE;
}

void radio_audio(struct Radio *radio, char volume, char flags)
{
    if (volume > 15 || (flags & 0xc0))
    {
        radio->power.error = APP_RET_ERR_INVALID_PARAM;
        return;
    }
    if (!ready(radio, false))
        return;
    char saved_direction = PEEK(DIRREG), saved_port = PEEK(PORTREG);
    unsigned int control = 0x8000;
    if (!(flags & RADIO_MUTE)) control |= 0x4000;
    if (flags & RADIO_MONO) control |= 0x2000;
    if (flags & RADIO_BASS) control |= 0x1000;
    unsigned int audio = 0;
    if (flags & RADIO_DE50) audio |= 0x0800;
    if (flags & RADIO_SOFTMUTE) audio |= 0x0200;
    if (!(flags & RADIO_AFC)) audio |= 0x0100;
    radio->power.stage = RADIO_POWER_WRITE;
    app_ret_code_t error = update_word(2, 0xf000, control);
    if (error == APP_RET_SUCCESS) error = update_word(4, 0x0b00, audio);
    if (error == APP_RET_SUCCESS) error = update_word(5, 0x000f, volume);
    POKE(PORTREG, saved_port);
    POKE(DIRREG, saved_direction);
    if (error == APP_RET_SUCCESS)
        radio_refresh(radio);
    else
    {
        radio->power.error = error;
        radio->valid = false;
    }
}

void radio_tune(struct Radio *radio, unsigned int frequency)
{
    if (frequency < RADIO_MIN || frequency > RADIO_MAX)
    {
        radio->power.error = APP_RET_ERR_OUT_OF_RANGE;
        return;
    }
    if (!ready(radio, false))
        return;
    char saved_direction = PEEK(DIRREG), saved_port = PEEK(PORTREG);
    unsigned int channel, mode;
    app_ret_code_t error = read_word(3, &channel);
    if (error == APP_RET_SUCCESS) error = read_word(7, &mode);
    if (error == APP_RET_SUCCESS && ((channel & 0x10) || (mode & 1)))
        error = APP_RET_ERR_INVALID_STATE;
    if (error == APP_RET_SUCCESS)
    {
        unsigned int target = ((frequency - RADIO_MIN) << 6) | 0x10;
        char bytes[2] = {target >> 8, target};
        error = i2c_write_register(0x11, 3, 2, bytes);
        if (error == APP_RET_SUCCESS) error = read_word(3, &channel);
        if (error == APP_RET_SUCCESS && (channel & 0xffef) != (target & 0xffef))
            error = APP_RET_ERR_INVALID_STATE;
    }
    POKE(PORTREG, saved_port);
    POKE(DIRREG, saved_direction);
    radio->power.stage = RADIO_POWER_WRITE;
    radio->power.error = error;
    if (error == APP_RET_SUCCESS)
    {
        radio->operation = RADIO_TUNING;
        radio->elapsed = 0;
        radio->frequency_valid = false;
    }
    else
        radio->valid = false;
}

void radio_configure(struct Radio *radio, char volume, char flags, unsigned int frequency)
{
    if (frequency < RADIO_MIN || frequency > RADIO_MAX || volume > 15 || (flags & 0xc0))
    {
        radio->power.error = APP_RET_ERR_INVALID_PARAM;
        return;
    }
    radio_audio(radio, volume, flags);
    if (radio->power.error != APP_RET_SUCCESS)
        return;
    char saved_direction = PEEK(DIRREG), saved_port = PEEK(PORTREG);
    app_ret_code_t error = update_word(7, 1, 0);
    POKE(PORTREG, saved_port);
    POKE(DIRREG, saved_direction);
    radio->power.error = error;
    if (error == APP_RET_SUCCESS)
        radio_tune(radio, frequency);
    else
        radio->valid = false;
}

void radio_seek(struct Radio *radio, bool up)
{
    if (!ready(radio, false))
        return;
    if (!radio->frequency_valid)
    {
        radio->power.error = APP_RET_ERR_CONFIG;
        return;
    }
    char saved_direction = PEEK(DIRREG), saved_port = PEEK(PORTREG);
    unsigned int channel;
    app_ret_code_t error = read_word(3, &channel);
    if (error == APP_RET_SUCCESS && (channel & 0x3f))
        error = APP_RET_ERR_INVALID_STATE;
    if (error == APP_RET_SUCCESS)
        error = update_word(2, 0x0380, up ? 0x0380 : 0x0180);
    POKE(PORTREG, saved_port);
    POKE(DIRREG, saved_direction);
    radio->power.error = error;
    radio->power.stage = RADIO_POWER_WRITE;
    if (error == APP_RET_SUCCESS)
    {
        radio->operation = RADIO_SEEKING;
        radio->elapsed = 0;
        radio->frequency_valid = false;
    }
    else
        radio->valid = false;
}

void radio_cancel(struct Radio *radio)
{
    if (!ready(radio, true))
    {
        radio->operation = RADIO_IDLE;
        return;
    }
    char saved_direction = PEEK(DIRREG), saved_port = PEEK(PORTREG);
    app_ret_code_t error = APP_RET_ERR_INVALID_STATE;
    if (!(radio->power.control & 2))
    {
        error = update_word(2, 0x0100, 0);
        if (error == APP_RET_SUCCESS) error = update_word(3, 0x0010, 0);
    }
    POKE(PORTREG, saved_port);
    POKE(DIRREG, saved_direction);
    radio->operation = RADIO_IDLE;
    radio->power.error = error;
    if (error == APP_RET_SUCCESS)
        radio_refresh(radio);
    else
        radio->valid = false;
}

void radio_poll(struct Radio *radio)
{
    enum RadioOperation operation = radio->operation;
    radio_refresh(radio);
    if (operation == RADIO_IDLE)
        return;
    ++radio->elapsed;
    if (radio->valid &&
        !(radio->power.control & 0x0100) &&
        (radio->status_a & 0x4000))
    {
        char saved_direction = PEEK(DIRREG), saved_port = PEEK(PORTREG);
        unsigned int channel;
        app_ret_code_t error = read_word(3, &channel);
        POKE(PORTREG, saved_port);
        POKE(DIRREG, saved_direction);
        if (error == APP_RET_SUCCESS && !(channel & 0x10))
        {
            radio->operation = RADIO_IDLE;
            if (operation == RADIO_SEEKING && (radio->status_a & 0x2000))
                radio->power.error = APP_RET_ERR_OUT_OF_RANGE;
            return;
        }
        if (error != APP_RET_SUCCESS)
        {
            radio->power.error = error;
            radio->valid = false;
        }
    }
    if (radio->elapsed >= (operation == RADIO_TUNING ? 8 : 20))
    {
        radio_cancel(radio);
        if (radio->power.error == APP_RET_SUCCESS)
            radio->power.error = APP_RET_ERR_TIMEOUT;
    }
    else
        radio->frequency_valid = false;
}

unsigned int radio_scale_frequency(unsigned int x)
{
    if (x <= RADIO_SCALE_LEFT) return RADIO_MIN;
    if (x >= RADIO_SCALE_RIGHT) return RADIO_MAX;
    return RADIO_MIN + ((unsigned long)(x - RADIO_SCALE_LEFT) * (RADIO_MAX - RADIO_MIN) +
        (RADIO_SCALE_RIGHT - RADIO_SCALE_LEFT) / 2) / (RADIO_SCALE_RIGHT - RADIO_SCALE_LEFT);
}

unsigned int radio_scale_position(unsigned int frequency)
{
    if (frequency <= RADIO_MIN) return RADIO_SCALE_LEFT;
    if (frequency >= RADIO_MAX) return RADIO_SCALE_RIGHT;
    return RADIO_SCALE_LEFT + ((unsigned long)(frequency - RADIO_MIN) * (RADIO_SCALE_RIGHT - RADIO_SCALE_LEFT) +
        (RADIO_MAX - RADIO_MIN) / 2) / (RADIO_MAX - RADIO_MIN);
}

void radio_frequency_text(unsigned int frequency, char *text)
{
    char index = 0;
    if (frequency >= 1000)
        text[index++] = '1';
    text[index++] = '0' + (frequency / 100) % 10;
    text[index++] = '0' + (frequency / 10) % 10;
    text[index++] = '.';
    text[index++] = '0' + frequency % 10;
    text[index] = 0;
}

bool radio_parse_frequency(const char *text, unsigned int *frequency)
{
    unsigned int value = 0;
    char digits = 0;
    while (*text >= '0' && *text <= '9' && digits < 3)
    {
        value = value * 10 + *text++ - '0';
        ++digits;
    }
    if (digits < 2 || *text++ != '.' || *text < '0' || *text > '9')
        return false;
    value = value * 10 + *text++ - '0';
    if (*text || value < RADIO_MIN || value > RADIO_MAX)
        return false;
    *frequency = value;
    return true;
}
