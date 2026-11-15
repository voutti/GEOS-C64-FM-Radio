#include <geos.h>
#include <i2c.h>
#include "radio_probe.h"
#include "radio_icons.h"

#pragma compile("radio_probe.c")

GEOS_CALLBACK(power_clicked)
GEOS_CALLBACK(quit_clicked)
GEOS_CALLBACK(placeholder_clicked)

static struct RadioPower power;
static char icon_buffer[33];
static struct iconpic icon = {icon_buffer, 2, 25, 2, 16};
static const struct window power_area = {49, 64, 72, 180};
static const struct window log_area = {181, 197, 8, 311};
static const struct window frames[] = {
    {0, 199, 0, 319},
    {28, 92, 264, 298},
    {28, 108, 264, 298},
    {33, 77, 269, 277},
    {33, 77, 282, 290},
    {144, 147, 53, 275},
    {165, 168, 53, 275},
    {180, 180, 8, 311}
};

static const struct menu file_menu = {
    {15, 30, 0, 60},
    1 | VERTICAL,
    {{" Quit ", MENU_ACTION, {.action = quit_clicked_geos}}}
};

static const struct menu main_menu = {
    {0, 14, 0, 319},
    4 | HORIZONTAL,
    {
        {" File ", SUB_MENU, {.submenu = &file_menu}},
        {" Settings* ", MENU_ACTION, {.action = placeholder_clicked_geos}},
        {" Channel* ", MENU_ACTION, {.action = placeholder_clicked_geos}},
        {" About* ", MENU_ACTION, {.action = placeholder_clicked_geos}}
    }
};

/* Only Power has a hit target; the other icons are visual placeholders. */
static const struct icontab power_icons = {
    1, 160, 100,
    {{tool_pictures[0], 2, 25, 2, 16, power_clicked_geos}}
};

static void draw_icon(const char *picture, char x, char y, bool selected)
{
    icon_buffer[0] = 0xa0;
    for (char index = 0; index != 32; ++index)
    {
        char value = picture[index + 1];
        if (selected && index >= 2 && index < 30)
            value ^= (index & 1) ? 0xfe : 0x7f;
        icon_buffer[index + 1] = value;
    }
    icon.x = x;
    icon.y = y;
    BitmapUp(&icon);
}

static void draw_power(void)
{
    GeosClearArea(&power_area);
    const char *state = "Power: unknown";
    if (power.state == RADIO_POWER_ON)
        state = "Power: ON";
    else if (power.state == RADIO_POWER_OFF)
        state = "Power: OFF";
    PutString(state, 60, 80);
    draw_icon(tool_pictures[0], 2, 25, power.state == RADIO_POWER_ON);

    GeosClearArea(&log_area);
    if (power.error == APP_RET_SUCCESS)
    {
        PutString("Power ready. Tuning and audio: next step.", 193, 10);
        return;
    }
    const char *message = "I2C bus init failed";
    if (power.stage == RADIO_PROBE_ID)
        message = "Tuner ID read / match failed";
    else if (power.stage == RADIO_POWER_CONTROL)
        message = "Control read failed / tuner busy";
    else if (power.stage == RADIO_POWER_WRITE)
        message = "Power write failed";
    else if (power.stage == RADIO_POWER_VERIFY)
        message = "Power readback failed";
    PutString(message, 193, 10);
    PutString("$", 193, 274);
    char error[5];
    radio_format_hex(power.error, error);
    PutString(error, 193, 282);
}

static void draw_gui(void)
{
    GeosClearScreen();
    for (char index = 0; index != sizeof(frames) / sizeof(frames[0]); ++index)
        GeosFrameArea(&frames[index]);

    for (char index = 1; index != 11; ++index)
        draw_icon(tool_pictures[index], 2 + (index & 1) * 2, 25 + (index / 2) * 16, false);
    for (char index = 0; index != 10; ++index)
        draw_icon(preset_pictures[index], 9 + index * 2, 25, false);
    static const char scan_x[] = {12, 17, 22, 27};
    for (char index = 0; index != 4; ++index)
        draw_icon(scan_pictures[index], scan_x[index], 115, false);
    for (char index = 0; index != 2; ++index)
        draw_icon(indicator_pictures[index], 33 + index * 2, 93, false);

    PutString("Frequency: ---.- MHz", 81, 80);
    PutString("FM-Radio", 60, 190);
    PutString("No station tuned by this app", 99, 80);
    PutString("Other controls: next step", 138, 80);
    PutString("V", 88, 270);
    PutString("I", 88, 285);
    PutString("FM", 158, 16);
    PutString("88", 158, 57);
    PutString("92", 158, 96);
    PutString("95", 158, 126);
    PutString("98", 158, 156);
    PutString("102", 158, 194);
    PutString("106", 158, 232);
    PutString("108", 158, 257);
    PutString("MHz", 158, 287);
#ifdef I2C_IN_CASSETTE_PORT
#ifdef I2C_CAS_SDA_WRITE
    PutString("I2C: cassette SDA=WRITE SCL=SENSE", 178, 16);
#else
    PutString("I2C: cassette SDA=SENSE SCL=WRITE", 178, 16);
#endif
#elif SDA_PIN == 0 && SCL_PIN == 1
    PutString("I2C: user port SDA=0 SCL=1", 178, 16);
#elif SDA_PIN == 2 && SCL_PIN == 3
    PutString("I2C: user port SDA=2 SCL=3", 178, 16);
#else
#error "Select a supported I2C wiring variant."
#endif
}

void power_clicked(void)
{
    radio_toggle_power(&power);
    draw_power();
}

void quit_clicked(void)
{
    RecoverMenu();
    EnterDeskTop();
}

void placeholder_clicked(void)
{
    RecoverMenu();
    GeosClearArea(&log_area);
    PutString("Not implemented yet. Use Power or Quit.", 193, 10);
    DoMenu(&main_menu);
}

int main(void)
{
    draw_gui();
    radio_read_power(&power);
    DoIcons(&power_icons);
    draw_power();
    DoMenu(&main_menu);
    MainLoop();
    return 0;
}
