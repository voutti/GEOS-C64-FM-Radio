#include <geos.h>
#include <i2c.h>
#include <string.h>
#include "radio.h"
#include "settings.h"
#include "radio_icons.h"
#include "frequency_display.h"

#pragma compile("radio_probe.c")
#pragma compile("radio.c")
#pragma compile("settings.c")
#pragma compile("frequency_display.c")

static void tool_clicked(char tool);
static void preset_clicked(char preset);
static void scan_clicked(char scan);

#define TOOL_HANDLER(name, index) \
    void name(void) { tool_clicked(index); }
#define PRESET_HANDLER(name, index) \
    void name(void) { preset_clicked(index); }
#define SCAN_HANDLER(name, index) \
    void name(void) { scan_clicked(index); }

GEOS_CALLBACK(power_clicked)
GEOS_CALLBACK(store_clicked)
GEOS_CALLBACK(volume_down)
GEOS_CALLBACK(volume_up)
GEOS_CALLBACK(mute_clicked)
GEOS_CALLBACK(softmute_clicked)
GEOS_CALLBACK(emphasis_clicked)
GEOS_CALLBACK(bass_clicked)
GEOS_CALLBACK(mono_clicked)
GEOS_CALLBACK(afc_clicked)
GEOS_CALLBACK(loop_clicked)
GEOS_CALLBACK(preset_1)
GEOS_CALLBACK(preset_2)
GEOS_CALLBACK(preset_3)
GEOS_CALLBACK(preset_4)
GEOS_CALLBACK(preset_5)
GEOS_CALLBACK(preset_6)
GEOS_CALLBACK(preset_7)
GEOS_CALLBACK(preset_8)
GEOS_CALLBACK(preset_9)
GEOS_CALLBACK(preset_10)
GEOS_CALLBACK(seek_down)
GEOS_CALLBACK(step_down)
GEOS_CALLBACK(step_up)
GEOS_CALLBACK(seek_up)
TOOL_HANDLER(power_clicked, 0)
TOOL_HANDLER(store_clicked, 1)
TOOL_HANDLER(volume_down, 2)
TOOL_HANDLER(volume_up, 3)
TOOL_HANDLER(mute_clicked, 4)
TOOL_HANDLER(softmute_clicked, 5)
TOOL_HANDLER(emphasis_clicked, 6)
TOOL_HANDLER(bass_clicked, 7)
TOOL_HANDLER(mono_clicked, 8)
TOOL_HANDLER(afc_clicked, 9)
TOOL_HANDLER(loop_clicked, 10)
PRESET_HANDLER(preset_1, 0)
PRESET_HANDLER(preset_2, 1)
PRESET_HANDLER(preset_3, 2)
PRESET_HANDLER(preset_4, 3)
PRESET_HANDLER(preset_5, 4)
PRESET_HANDLER(preset_6, 5)
PRESET_HANDLER(preset_7, 6)
PRESET_HANDLER(preset_8, 7)
PRESET_HANDLER(preset_9, 8)
PRESET_HANDLER(preset_10, 9)
SCAN_HANDLER(seek_down, 0)
SCAN_HANDLER(step_down, 1)
SCAN_HANDLER(step_up, 2)
SCAN_HANDLER(seek_up, 3)
GEOS_CALLBACK(save_clicked)
GEOS_CALLBACK(load_clicked)
GEOS_CALLBACK(quit_clicked)
GEOS_CALLBACK(wiring_clicked)
GEOS_CALLBACK(reinit_clicked)
GEOS_CALLBACK(reset_clicked)
GEOS_CALLBACK(tune_clicked)
GEOS_CALLBACK(cancel_clicked)
GEOS_CALLBACK(about_clicked)
GEOS_CALLBACK(idle_tick)
GEOS_CALLBACK(scale_clicked)

static struct Radio radio;
static struct Settings settings;
static bool store_active, loop_active, dirty;
static char loop_index = 0xff, loop_seconds, status_seconds, initialize_ticks;
static char tool_images[11][33], preset_images[10][33], scratch_image[33];
static bool display_ready, displayed_frequency_valid, displayed_marker_valid;
static bool displayed_stereo, displayed_tuned;
static unsigned int displayed_frequency, displayed_marker, displayed_tools, displayed_presets;
static unsigned int displayed_stored_presets;
static char displayed_volume, displayed_signal, displayed_station[16];
static char displayed_volume_height, displayed_signal_height;
static const char *displayed_pictures[11];
static char preset_labels[10][22];
static const struct window log_area = {181, 197, 8, 311};
static const struct window station_area = {100, 112, 72, 254};
static const struct window meter_volume = {34, 76, 270, 276};
static const struct window meter_signal = {34, 76, 283, 289};
static const struct window scale_area = {165, 168, RADIO_SCALE_LEFT, RADIO_SCALE_RIGHT};
static const struct window scale_clear = {162, 170, RADIO_SCALE_LEFT, RADIO_SCALE_RIGHT};
static const struct window scale_hit_area = {144, 170, RADIO_SCALE_LEFT, RADIO_SCALE_RIGHT + 9};
static const struct window frames[] = {
    {0, 199, 0, 319}, {28, 92, 264, 298}, {28, 108, 264, 298},
    {33, 77, 269, 277}, {33, 77, 282, 290},
    {144, 147, RADIO_SCALE_LEFT, RADIO_SCALE_RIGHT},
    {165, 168, RADIO_SCALE_LEFT, RADIO_SCALE_RIGHT}, {180, 180, 8, 311}
};

static const struct menu file_menu = {
    {15, 60, 0, 112}, 3 | VERTICAL,
    {
        {"Save settings", MENU_ACTION, {.action = save_clicked_geos}},
        {"Load settings", MENU_ACTION, {.action = load_clicked_geos}},
        {"Quit", MENU_ACTION, {.action = quit_clicked_geos}}
    }
};
static const struct menu settings_menu = {
    {15, 60, 40, 166}, 3 | VERTICAL,
    {
        {"Wiring", MENU_ACTION, {.action = wiring_clicked_geos}},
        {"Reinitialize", MENU_ACTION, {.action = reinit_clicked_geos}},
        {"Reset settings", MENU_ACTION, {.action = reset_clicked_geos}}
    }
};
static const struct menu channel_menu = {
    {15, 195, 96, 296}, 12 | VERTICAL,
    {
        {"Manual tune", MENU_ACTION, {.action = tune_clicked_geos}},
        {preset_labels[0], MENU_ACTION, {.action = preset_1_geos}},
        {preset_labels[1], MENU_ACTION, {.action = preset_2_geos}},
        {preset_labels[2], MENU_ACTION, {.action = preset_3_geos}},
        {preset_labels[3], MENU_ACTION, {.action = preset_4_geos}},
        {preset_labels[4], MENU_ACTION, {.action = preset_5_geos}},
        {preset_labels[5], MENU_ACTION, {.action = preset_6_geos}},
        {preset_labels[6], MENU_ACTION, {.action = preset_7_geos}},
        {preset_labels[7], MENU_ACTION, {.action = preset_8_geos}},
        {preset_labels[8], MENU_ACTION, {.action = preset_9_geos}},
        {preset_labels[9], MENU_ACTION, {.action = preset_10_geos}},
        {"Cancel tune/seek", MENU_ACTION, {.action = cancel_clicked_geos}}
    }
};
static const struct menu main_menu = {
    {0, 14, 0, 319}, 4 | HORIZONTAL,
    {
        {" File ", SUB_MENU, {.submenu = &file_menu}},
        {" Settings ", SUB_MENU, {.submenu = &settings_menu}},
        {" Channel ", SUB_MENU, {.submenu = &channel_menu}},
        {" About ", MENU_ACTION, {.action = about_clicked_geos}}
    }
};

#define TOOL_ICON(index, callback) \
    {tool_images[index], 2 + ((index) & 1) * 2, 25 + ((index) / 2) * 16, 2, 16, callback##_geos}
#define PRESET_ICON(index, callback) \
    {preset_images[index], 9 + (index) * 2, 25, 2, 16, callback##_geos}
static const struct icontab radio_icons = {
    25, 160, 100,
    {
        TOOL_ICON(0, power_clicked), TOOL_ICON(1, store_clicked),
        TOOL_ICON(2, volume_down), TOOL_ICON(3, volume_up), TOOL_ICON(4, mute_clicked),
        TOOL_ICON(5, softmute_clicked), TOOL_ICON(6, emphasis_clicked),
        TOOL_ICON(7, bass_clicked), TOOL_ICON(8, mono_clicked),
        TOOL_ICON(9, afc_clicked), TOOL_ICON(10, loop_clicked),
        PRESET_ICON(0, preset_1), PRESET_ICON(1, preset_2),
        PRESET_ICON(2, preset_3), PRESET_ICON(3, preset_4),
        PRESET_ICON(4, preset_5), PRESET_ICON(5, preset_6),
        PRESET_ICON(6, preset_7), PRESET_ICON(7, preset_8),
        PRESET_ICON(8, preset_9), PRESET_ICON(9, preset_10),
        {scan_pictures[0], 12, 115, 2, 16, seek_down_geos},
        {scan_pictures[1], 17, 115, 2, 16, step_down_geos},
        {scan_pictures[2], 22, 115, 2, 16, step_up_geos},
        {scan_pictures[3], 27, 115, 2, 16, seek_up_geos}
    }
};

static void log_message(const char *text)
{
    GeosClearArea(&log_area);
    PutString(text, 193, 10);
}

static void log_error(const char *text, unsigned int error)
{
    log_message(text);
    char hex[5];
    radio_format_hex(error, hex);
    PutString("$", 193, 274);
    PutString(hex, 193, 282);
}

static void release_menu(void)
{
    RecoverMenu();
    DoMenu(&main_menu);
}

static void update_labels(void)
{
    for (char index = 0; index != 10; ++index)
    {
        char *label = preset_labels[index];
        if (index == 9)
        {
            *label++ = '1';
            *label++ = '0';
        }
        else
            *label++ = '1' + index;
        *label++ = ':';
        *label++ = ' ';
        if (!settings.presets[index].frequency)
            strcpy(label, "(empty)");
        else if (settings.presets[index].name[0])
            strcpy(label, settings.presets[index].name);
        else
            radio_frequency_text(settings.presets[index].frequency, label);
    }
}

static void bitmap(const char *source, char *image, char x, char y, bool selected)
{
    memcpy(image, source, 33);
    if (selected)
        for (char index = 2; index < 30; ++index)
            image[index + 1] ^= (index & 1) ? 0xfe : 0x7f;
    struct iconpic picture = {image, x, y, 2, 16};
    BitmapUp(&picture);
}

static char meter(const struct window *area, char value, char maximum, char previous_height)
{
    char height = (unsigned int)value * (area->bottom - area->top + 1) / maximum;
    if (!display_ready)
    {
        GeosClearArea(area);
        previous_height = 0;
    }
    if (height != previous_height)
    {
        struct window bar = *area;
        if (height > previous_height)
        {
            bar.top = area->bottom - height + 1;
            bar.bottom = area->bottom - previous_height;
            GeosFillArea(&bar);
        }
        else
        {
            bar.top = area->bottom - previous_height + 1;
            bar.bottom = area->bottom - height;
            GeosClearArea(&bar);
        }
    }
    return height;
}

static void draw_state(void)
{
    static const char flag_for_tool[11] = {
        0, 0, 0, 0, RADIO_MUTE, RADIO_SOFTMUTE, 0, RADIO_BASS, 0, RADIO_AFC, 0
    };
    unsigned int selected_tools = 0, selected_presets = 0, stored_presets = 0;
    for (char index = 0; index != 11; ++index)
    {
        bool selected = radio.valid && (radio.flags & flag_for_tool[index]);
        if (index == 0) selected = radio.power.state == RADIO_POWER_ON;
        if (index == 1) selected = store_active;
        if (index == 10) selected = loop_active;
        const char *picture = tool_pictures[index];
        if (index == 6 && radio.valid && !(radio.flags & RADIO_DE50))
            picture = emphasis_75_picture;
        if (index == 8 && radio.valid && (radio.flags & RADIO_MONO))
            picture = mono_picture;
        unsigned int bit = 1u << index;
        if (selected) selected_tools |= bit;
        if (!display_ready || picture != displayed_pictures[index] ||
            ((selected_tools ^ displayed_tools) & bit))
            bitmap(picture, tool_images[index], 2 + (index & 1) * 2,
                   25 + (index / 2) * 16, selected);
        displayed_pictures[index] = picture;
    }
    displayed_tools = selected_tools;
    const char *station = "";
    for (char index = 0; index != 10; ++index)
    {
        bool selected = radio.valid && radio.frequency_valid &&
            settings.presets[index].frequency == radio.frequency;
        if (selected)
            station = settings.presets[index].name;
        unsigned int bit = 1u << index;
        if (selected) selected_presets |= bit;
        bool stored = settings.presets[index].frequency != 0;
        if (stored) stored_presets |= bit;
        if (!display_ready || ((stored_presets ^ displayed_stored_presets) & bit))
        {
            struct window line = {22, 22, 75 + index * 16, 84 + index * 16};
            if (stored) GeosFillArea(&line);
            else GeosClearArea(&line);
        }
        if (!display_ready || ((selected_presets ^ displayed_presets) & bit))
            bitmap(preset_pictures[index], preset_images[index], 9 + index * 2, 25, selected);
    }
    displayed_presets = selected_presets;
    displayed_stored_presets = stored_presets;
    bool frequency_valid = radio.valid && radio.frequency_valid;
    if (!display_ready || frequency_valid != displayed_frequency_valid ||
        (frequency_valid && radio.frequency != displayed_frequency))
        draw_frequency(radio.frequency, frequency_valid);
    displayed_frequency = radio.frequency;
    displayed_frequency_valid = frequency_valid;
    if (!display_ready || strcmp(station, displayed_station))
    {
        GeosClearArea(&station_area);
        PutString(station, 109, 80);
        strcpy(displayed_station, station);
    }
    bool on = radio.valid && radio.power.state == RADIO_POWER_ON;
    char volume = on ? radio.volume : 0;
    char signal = on ? radio.status_b >> 9 : 0;
    if (!display_ready || volume != displayed_volume)
        displayed_volume_height = meter(&meter_volume, volume, 15, displayed_volume_height);
    if (!display_ready || signal != displayed_signal)
        displayed_signal_height = meter(&meter_signal, signal, 127, displayed_signal_height);
    displayed_volume = volume;
    displayed_signal = signal;
    bool stereo = on && (radio.status_a & 0x0400);
    bool tuned = on && (radio.status_b & 0x0100);
    if (!display_ready || stereo != displayed_stereo)
        bitmap(stereo ? tool_pictures[8] : mono_picture, scratch_image, 33, 93, false);
    if (!display_ready || tuned != displayed_tuned)
        bitmap(tuned ? tuned_picture : indicator_pictures[1], scratch_image, 35, 93, false);
    displayed_stereo = stereo;
    displayed_tuned = tuned;
    bool marker_valid = on && radio.frequency_valid;
    if (!display_ready || marker_valid != displayed_marker_valid ||
        (marker_valid && radio.frequency != displayed_marker))
    {
        GeosClearArea(&scale_clear);
        GeosFrameArea(&scale_area);
        if (marker_valid)
        {
            unsigned int x = radio_scale_position(radio.frequency);
            struct window marker = {163, 170, x, x};
            GeosFrameArea(&marker);
        }
    }
    displayed_marker = radio.frequency;
    displayed_marker_valid = marker_valid;
    display_ready = true;
}

static void remember_live(void)
{
    if (!radio.valid)
        return;
    if (settings.volume != radio.volume || settings.flags != radio.flags ||
        (radio.frequency_valid && settings.frequency != radio.frequency))
        dirty = true;
    settings.volume = radio.volume;
    settings.flags = radio.flags;
    if (radio.frequency_valid)
        settings.frequency = radio.frequency;
}

static bool usable(void)
{
    if (initialize_ticks)
    {
        log_message("Radio is starting. Please wait.");
        return false;
    }
    if (!radio.valid || radio.power.state != RADIO_POWER_ON)
    {
        log_message("Power on or Reinitialize the radio first.");
        return false;
    }
    if (radio.operation != RADIO_IDLE)
    {
        log_message("Busy. Channel / Cancel tune/seek to stop.");
        return false;
    }
    return true;
}

static void tune_to(unsigned int frequency)
{
    radio_tune(&radio, frequency);
    draw_state();
    if (radio.power.error)
        log_error("Tune failed", radio.power.error);
    else
        log_message("Tuning...");
}

void scale_clicked(void)
{
    unsigned int x;
    char y;
    GeosGetMousePosition(&x, &y);
    if ((PEEK(0x8505) & 0x80) || PEEK(0x84b7) ||
        x < scale_hit_area.left || x > scale_hit_area.right ||
        y < scale_hit_area.top || y > scale_hit_area.bottom)
        return;
    if (!usable())
        return;
    loop_active = store_active = false;
    tune_to(radio_scale_frequency(x));
}

static bool next_preset(void)
{
    for (char attempt = 0; attempt != 10; ++attempt)
    {
        loop_index = loop_index >= 9 ? 0 : loop_index + 1;
        if (settings.presets[loop_index].frequency)
        {
            tune_to(settings.presets[loop_index].frequency);
            return radio.power.error == APP_RET_SUCCESS;
        }
    }
    log_message("No stored presets to loop.");
    return false;
}

static void tool_clicked(char tool)
{
    if (tool == 0)
    {
        loop_active = store_active = false;
        initialize_ticks = 0;
        if (radio.operation != RADIO_IDLE)
        {
            radio_cancel(&radio);
            if (radio.power.error)
            {
                log_error("Cancel failed", radio.power.error);
                draw_state();
                return;
            }
        }
        radio_toggle_power(&radio.power);
        if (!radio.power.error && radio.power.state == RADIO_POWER_ON)
        {
            initialize_ticks = 2;
            radio.valid = false;
        }
        else if (!radio.power.error)
            radio_refresh(&radio);
        else
            radio.valid = false;
        draw_state();
        if (radio.power.error) log_error("Power / initialization failed", radio.power.error);
        else if (initialize_ticks) log_message("Power on. Starting oscillator...");
        else log_message("Power off.");
        return;
    }
    if (!usable())
        return;
    if (tool == 1)
    {
        loop_active = false;
        store_active = !store_active;
        log_message(store_active ? "Choose preset 1-10 to store this station." : "Store cancelled.");
    }
    else if (tool == 10)
    {
        store_active = false;
        loop_active = !loop_active;
        loop_seconds = 0;
        if (loop_active)
        {
            loop_index = 0xff;
            loop_active = next_preset();
        }
        else
            log_message("Preset loop stopped.");
    }
    else
    {
        char volume = radio.volume, flags = radio.flags;
        if (tool == 2 && volume) --volume;
        if (tool == 3 && volume < 15) ++volume;
        if (tool == 4) flags ^= RADIO_MUTE;
        if (tool == 5) flags ^= RADIO_SOFTMUTE;
        if (tool == 6) flags ^= RADIO_DE50;
        if (tool == 7) flags ^= RADIO_BASS;
        if (tool == 8) flags ^= RADIO_MONO;
        if (tool == 9) flags ^= RADIO_AFC;
        radio_audio(&radio, volume, flags);
        if (radio.power.error) log_error("Audio change failed", radio.power.error);
        else
            remember_live();
    }
    draw_state();
}

static void preset_clicked(char preset)
{
    if (PEEK(0x84b7))
        release_menu();
    if (!usable())
        return;
    loop_active = false;
    if (!store_active)
    {
        if (!settings.presets[preset].frequency)
        {
            log_message("Empty preset. Use Store, then select 1-10.");
            draw_state();
        }
        else
            tune_to(settings.presets[preset].frequency);
        return;
    }
    if (!radio.frequency_valid)
    {
        log_message("Tune a station before storing a preset.");
        return;
    }
    char name[16] = {0};
    strcpy(name, settings.presets[preset].name);
    char reply = GeosInput("Station name (15 characters)", "Enter to store; Cancel to discard", name, 15);
    store_active = false;
    if (reply != GEOS_CANCEL)
    {
        bool valid = true;
        for (char index = 0; name[index]; ++index)
            if (name[index] < 32 || name[index] > 126) valid = false;
        if (!valid)
            log_message("Use plain ASCII characters for the name.");
        else
        {
            settings.presets[preset].frequency = radio.frequency;
            strcpy(settings.presets[preset].name, name);
            update_labels();
            dirty = true;
            log_message("Preset stored. File / Save settings to keep.");
        }
    }
    else
        log_message("Store cancelled.");
    draw_state();
    DoMenu(&main_menu);
}

static void scan_clicked(char scan)
{
    if (!usable())
        return;
    store_active = loop_active = false;
    if (scan == 0 || scan == 3)
    {
        radio_seek(&radio, scan == 3);
        draw_state();
        if (radio.power.error) log_error("Seek failed", radio.power.error);
        else log_message("Seeking... Channel / Cancel to stop.");
    }
    else if (!radio.frequency_valid)
        log_message("Use Manual tune or Reinitialize first.");
    else if ((scan == 1 && radio.frequency == RADIO_MIN) ||
             (scan == 2 && radio.frequency == RADIO_MAX))
        log_message("Band limit reached (87.0-108.0 MHz).");
    else
        tune_to(scan == 1 ? radio.frequency - 1 : radio.frequency + 1);
}

void save_clicked(void)
{
    release_menu();
    char error = settings_save(&settings);
    if (error) log_error("Save failed; previous slot retained", error);
    else
    {
        dirty = false;
        log_message("Settings saved and verified on this disk.");
    }
}

void load_clicked(void)
{
    release_menu();
    char error = settings_load(&settings);
    if (error)
    {
        log_error("Settings load failed", error);
        return;
    }
    loop_active = store_active = false;
    dirty = false;
    update_labels();
    bool apply = radio.power.state == RADIO_POWER_ON && !initialize_ticks;
    if (apply)
    {
        radio_cancel(&radio);
        if (!radio.power.error)
            radio_configure(&radio, settings.volume, settings.flags, settings.frequency);
    }
    draw_state();
    if (apply && radio.power.error)
        log_error("Loaded; apply failed", radio.power.error);
    else if (settings.warning) log_error("Loaded backup; other slot invalid", settings.warning);
    else log_message("Settings loaded.");
}

void quit_clicked(void)
{
    release_menu();
    if (dirty && GeosConfirm("Unsaved presets / settings", "Quit without saving?") != GEOS_YES)
        return;
    if (radio.operation != RADIO_IDLE)
    {
        radio_cancel(&radio);
        if (radio.power.error)
        {
            draw_state();
            log_error("Cancel before Quit failed", radio.power.error);
            return;
        }
    }
    initialize_ticks = 0;
    EnterDeskTop();
}

static const char wiring_description[] =
#ifdef I2C_IN_CASSETTE_PORT
#ifdef I2C_CAS_SDA_WRITE
    "Cassette: SDA=WRITE(3) SCL=SENSE(4)";
#else
    "Cassette: SDA=SENSE(4) SCL=WRITE(3)";
#endif
#elif SDA_PIN == 0 && SCL_PIN == 1
    "User port: SDA=PB0 SCL=PB1";
#else
    "User port: SDA=PB2 SCL=PB3";
#endif

void wiring_clicked(void)
{
    release_menu();
    GeosMessage(wiring_description, "Wiring is fixed by the build variant.");
    DoMenu(&main_menu);
}

void reinit_clicked(void)
{
    release_menu();
    loop_active = store_active = false;
    initialize_ticks = 0;
    radio_read_power(&radio.power);
    if (!radio.power.error && radio.power.state == RADIO_POWER_ON)
        radio_cancel(&radio);
    if (!radio.power.error && radio.power.state == RADIO_POWER_OFF)
        radio_toggle_power(&radio.power);
    if (!radio.power.error)
    {
        initialize_ticks = 2;
        radio.valid = false;
    }
    draw_state();
    if (radio.power.error) log_error("Reinitialize failed", radio.power.error);
    else log_message("Radio restarting...");
}

void reset_clicked(void)
{
    release_menu();
    if (GeosConfirm("Clear presets and restore defaults?", "Use File / Save to make permanent.") != GEOS_YES)
        return;
    unsigned int generation = settings.generation;
    char slot = settings.slot;
    settings_default(&settings);
    settings.generation = generation;
    settings.slot = slot;
    loop_active = store_active = false;
    dirty = true;
    update_labels();
    bool apply = radio.power.state == RADIO_POWER_ON && !initialize_ticks;
    if (apply)
    {
        radio_cancel(&radio);
        if (!radio.power.error)
            radio_configure(&radio, settings.volume, settings.flags, settings.frequency);
    }
    draw_state();
    if (apply && radio.power.error)
        log_error("Defaults restored; radio apply failed", radio.power.error);
    else log_message("Defaults restored. File / Save to keep.");
    DoMenu(&main_menu);
}

void tune_clicked(void)
{
    release_menu();
    if (!usable())
        return;
    char text[8];
    radio_frequency_text(radio.frequency_valid ? radio.frequency : settings.frequency, text);
    if (GeosInput("Frequency, e.g. 88.6 or 100.1", "Range: 87.0-108.0 MHz", text, 5) == GEOS_CANCEL)
        return;
    unsigned int frequency;
    if (!radio_parse_frequency(text, &frequency))
        log_message("Invalid frequency. Use 87.0-108.0 MHz.");
    else
    {
        loop_active = store_active = false;
        tune_to(frequency);
    }
    DoMenu(&main_menu);
}

void cancel_clicked(void)
{
    release_menu();
    loop_active = store_active = false;
    initialize_ticks = 0;
    radio_cancel(&radio);
    draw_state();
    if (radio.power.error) log_error("Cancel failed", radio.power.error);
    else log_message("Tune / seek cancelled.");
}

void about_clicked(void)
{
    release_menu();
    GeosMessage("FM-Radio " APP_VERSION " / RDA5807 / GEOS 64", wiring_description);
    DoMenu(&main_menu);
}

void idle_tick(void)
{
    if (initialize_ticks)
    {
        if (--initialize_ticks)
            return;
        radio_configure(&radio, settings.volume, settings.flags, settings.frequency);
        draw_state();
        if (radio.power.error) log_error("Initialization failed", radio.power.error);
        else log_message("Audio configured. Tuning...");
        return;
    }
    if ((!radio.valid && radio.operation == RADIO_IDLE) || radio.power.state != RADIO_POWER_ON)
        return;
    enum RadioOperation previous = radio.operation;
    bool poll = previous != RADIO_IDLE || ++status_seconds >= 3;
    if (poll)
    {
        status_seconds = 0;
        radio_poll(&radio);
    }
    if (radio.power.error)
    {
        loop_active = false;
        log_error("Radio status / tune / seek failed", radio.power.error);
    }
    else if (previous != RADIO_IDLE && radio.operation == RADIO_IDLE)
    {
        loop_seconds = 0;
        remember_live();
        log_message(loop_active ? "Preset loop: 5 seconds per station." : "Station tuned.");
    }
    if (loop_active && previous == RADIO_IDLE && radio.operation == RADIO_IDLE &&
        !radio.power.error && ++loop_seconds >= 5)
    {
        loop_seconds = 0;
        loop_active = next_preset();
    }
    draw_state();
}

static void draw_gui(void)
{
    GeosClearScreen();
    for (char index = 0; index != sizeof(frames) / sizeof(frames[0]); ++index)
        GeosFrameArea(&frames[index]);
    static const char scan_x[] = {12, 17, 22, 27};
    for (char index = 0; index != 4; ++index)
        bitmap(scan_pictures[index], scratch_image, scan_x[index], 115, false);
    PutString("100 kHz steps / automatic seek", 138, 80);
    PutString("V", 88, 270);
    PutString("I", 88, 285);
    PutString("FM-Radio", 60, 190);
    PutString("MHz", 94, 232);
    PutString("FM", 158, 16);
    PutString("88", 158, radio_scale_position(880) - 6);
    PutString("92", 158, radio_scale_position(920) - 6);
    PutString("95", 158, radio_scale_position(950) - 6);
    PutString("98", 158, radio_scale_position(980) - 6);
    PutString("102", 158, radio_scale_position(1020) - 9);
    PutString("106", 158, radio_scale_position(1060) - 9);
    PutString("108", 158, radio_scale_position(1080) - 9);
    PutString("MHz", 158, 287);
}

int main(void)
{
    settings_default(&settings);
    char load_error = settings_load(&settings);
    update_labels();
    radio_refresh(&radio);
    draw_gui();
    draw_state();
    DoIcons(&radio_icons);
    if (load_error && load_error != GEOS_FILE_NOT_FOUND)
        log_error("Settings unavailable; using defaults", load_error);
    else if (settings.warning)
        log_error("Recovered settings backup", settings.warning);
    else if (radio.power.error)
        log_error("Tuner read failed; Power to retry", radio.power.error);
    else
        log_message("Ready. Power on to apply saved settings.");
    DoMenu(&main_menu);
    GeosSetSecondAction(idle_tick_geos);
    GeosSetOtherPressAction(scale_clicked_geos);
    MainLoop();
    return 0;
}
