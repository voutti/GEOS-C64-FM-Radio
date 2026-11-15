#include <geos.h>
#include "radio.h"
#include "frequency_display.h"

static const char frequency_digits[10][8] = {
    {0x7e,0xff,0xe7,0xe7,0xe7,0xe7,0xff,0x7e},
    {0x1c,0x3c,0x7c,0x1c,0x1c,0x1c,0x1c,0x1c},
    {0x7e,0xff,0xe7,0x0e,0x38,0xe0,0xff,0xff},
    {0xfe,0xff,0x07,0xfe,0xfe,0x07,0xff,0xfe},
    {0xee,0xee,0xee,0xff,0xff,0x0e,0x0e,0x0e},
    {0xff,0xff,0xe0,0xfe,0xff,0x07,0xff,0x7e},
    {0x7e,0xff,0xe0,0xfe,0xff,0xe7,0xff,0x7e},
    {0xff,0xff,0x07,0x0e,0x1c,0x38,0x38,0x38},
    {0x7e,0xff,0xe7,0x7e,0x7e,0xe7,0xff,0x7e},
    {0x7e,0xff,0xe7,0xff,0x7f,0x07,0xff,0x7e}
};
static char digit_image[32 * (1 + 4 * DISPLAY_X_SCALE)];
static const struct window frequency_area = {65, 96, DISPLAY_X(72), DISPLAY_RIGHT(223)};
static const struct window decimal_point = {93, 96, DISPLAY_X(184), DISPLAY_RIGHT(187)};

static void draw_digit(char number, char position)
{
    unsigned int index = 0;
    for (char row = 0; row != 8; ++row)
        for (char repeat = 0; repeat != 4; ++repeat)
        {
            // Each GEOS bitmap scanline is a literal run of four bytes.
            digit_image[index++] = 0x80 + 4 * DISPLAY_X_SCALE;
            for (char pair = 0; pair != 4 * DISPLAY_X_SCALE; ++pair)
            {
                char pixels = frequency_digits[number][row];
#ifdef GEOS_C128_80
                char expanded = pixels & (0x80 >> pair) ? 0xff : 0;
#else
                char expanded = pixels & (0x80 >> (pair * 2)) ? 0xf0 : 0;
                if (pixels & (0x40 >> (pair * 2))) expanded |= 0x0f;
#endif
                digit_image[index++] = expanded;
            }
        }
    struct iconpic picture = {
        digit_image, (9 + position * 5) * DISPLAY_X_SCALE, 65,
        4 * DISPLAY_X_SCALE, 32
    };
    BitmapUp(&picture);
}

void draw_frequency(unsigned int frequency, bool valid)
{
    GeosClearArea(&frequency_area);
    if (valid && frequency >= RADIO_MIN && frequency <= RADIO_MAX)
    {
        if (frequency >= 1000)
            draw_digit(frequency / 1000, 0);
        draw_digit((frequency / 100) % 10, 1);
        draw_digit((frequency / 10) % 10, 2);
        draw_digit(frequency % 10, 3);
    }
    else
        for (char position = 0; position != 4; ++position)
        {
            struct window dash = {
                79, 82, DISPLAY_X(76 + position * 40), DISPLAY_RIGHT(99 + position * 40)
            };
            GeosFillArea(&dash);
        }
    GeosFillArea(&decimal_point);
}
