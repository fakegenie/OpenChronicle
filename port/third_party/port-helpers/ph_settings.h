#ifndef PH_SETTINGS_H
#define PH_SETTINGS_H

#include <SDL3/SDL.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    PH_SHOW_ALWAYS,
    PH_SHOW_DESKTOP,
    PH_SHOW_MOBILE,
    PH_SHOW_HIDDEN,
};

struct ph_setting {
    const char *key;
    const char *label;
    int section;
    int show;
    int *value;
    int fallback;
    const char *const *choices;
    int choice_count;
    int minimum;
    int maximum;
    int step;
    const char *unit;
    const char *zero;
    char *text;
    int text_size;
    const char *text_fallback;
};

extern const char *const ph_off_on[2];

void ph_settings_reset(const struct ph_setting *settings, int count);
int ph_settings_take(const struct ph_setting *settings, int count, const char *key, const char *value);
void ph_settings_write(SDL_IOStream *file, const struct ph_setting *settings, int count);
int ph_setting_find(const struct ph_setting *settings, int count, const char *key);
int ph_setting_shown(const struct ph_setting *setting);
int ph_setting_adjustable(const struct ph_setting *setting);
void ph_setting_step(const struct ph_setting *setting, int delta, int wrap);
void ph_setting_text(const struct ph_setting *setting, char *text, int size);
int ph_next_pair(char **cursor, char **key, char **value);

#ifdef __cplusplus
}
#endif

#endif

#if defined(PH_IMPLEMENTATION) && !defined(PH_SETTINGS_IMPLEMENTED)
#define PH_SETTINGS_IMPLEMENTED

#include "ph_view.h"

const char *const ph_off_on[2] = { "Off", "On" };

static void ph_setting_reset(const struct ph_setting *setting)
{
    if (setting->text) {
        SDL_strlcpy(setting->text, setting->text_fallback ? setting->text_fallback : "", (size_t)setting->text_size);
    } else if (setting->value) {
        *setting->value = setting->fallback;
    }
}

void ph_settings_reset(const struct ph_setting *settings, int count)
{
    for (int index = 0; index < count; index++) {
        ph_setting_reset(&settings[index]);
    }
}

int ph_setting_find(const struct ph_setting *settings, int count, const char *key)
{
    for (int index = 0; index < count; index++) {
        if (settings[index].key && !SDL_strcmp(settings[index].key, key)) {
            return index;
        }
    }
    return -1;
}

static void ph_setting_clamp(const struct ph_setting *setting)
{
    if (setting->choices) {
        *setting->value = SDL_clamp(*setting->value, 0, setting->choice_count - 1);
    } else if (setting->minimum < setting->maximum) {
        *setting->value = SDL_clamp(*setting->value, setting->minimum, setting->maximum);
    }
}

int ph_settings_take(const struct ph_setting *settings, int count, const char *key, const char *value)
{
    int index = ph_setting_find(settings, count, key);
    if (index < 0) {
        return 0;
    }
    const struct ph_setting *setting = &settings[index];
    if (setting->text) {
        SDL_strlcpy(setting->text, value, (size_t)setting->text_size);
        return 1;
    }
    if (!setting->value) {
        return 1;
    }
    for (int choice = 0; setting->choices && choice < setting->choice_count; choice++) {
        if (!SDL_strcasecmp(value, setting->choices[choice])) {
            *setting->value = choice;
            return 1;
        }
    }
    *setting->value = SDL_atoi(value);
    ph_setting_clamp(setting);
    return 1;
}

void ph_settings_write(SDL_IOStream *file, const struct ph_setting *settings, int count)
{
    for (int index = 0; index < count; index++) {
        const struct ph_setting *setting = &settings[index];
        if (!setting->key) {
            continue;
        }
        if (setting->text) {
            SDL_IOprintf(file, "%s=%s\n", setting->key, setting->text);
        } else if (setting->value && setting->choices) {
            SDL_IOprintf(file, "%s=%s\n", setting->key, setting->choices[SDL_clamp(*setting->value, 0, setting->choice_count - 1)]);
        } else if (setting->value) {
            SDL_IOprintf(file, "%s=%d\n", setting->key, *setting->value);
        }
    }
}

int ph_setting_shown(const struct ph_setting *setting)
{
    if (setting->show == PH_SHOW_HIDDEN || !setting->label) {
        return 0;
    }
    if (setting->show == PH_SHOW_DESKTOP) {
        return !PH_MOBILE;
    }
    if (setting->show == PH_SHOW_MOBILE) {
        return PH_MOBILE;
    }
    return 1;
}

int ph_setting_adjustable(const struct ph_setting *setting)
{
    return setting->value && !setting->text && (setting->choices ? setting->choice_count > 1 : setting->minimum < setting->maximum);
}

void ph_setting_step(const struct ph_setting *setting, int delta, int wrap)
{
    if (!ph_setting_adjustable(setting)) {
        return;
    }
    int low = setting->choices ? 0 : setting->minimum;
    int high = setting->choices ? setting->choice_count - 1 : setting->maximum;
    int step = setting->choices || setting->step <= 0 ? 1 : setting->step;
    int value = *setting->value + delta * step;
    if (wrap && value > high) {
        value = low;
    } else if (wrap && value < low) {
        value = high;
    }
    *setting->value = SDL_clamp(value, low, high);
}

void ph_setting_text(const struct ph_setting *setting, char *text, int size)
{
    if (setting->text) {
        SDL_strlcpy(text, setting->text, (size_t)size);
    } else if (!setting->value) {
        text[0] = 0;
    } else if (setting->choices) {
        SDL_strlcpy(text, setting->choices[SDL_clamp(*setting->value, 0, setting->choice_count - 1)], (size_t)size);
    } else if (*setting->value == 0 && setting->zero) {
        SDL_strlcpy(text, setting->zero, (size_t)size);
    } else {
        SDL_snprintf(text, (size_t)size, "%d%s", *setting->value, setting->unit ? setting->unit : "");
    }
}

static char *ph_trim(char *text)
{
    while (*text == ' ' || *text == '\t') {
        text++;
    }
    char *end = text + SDL_strlen(text);
    while (end > text && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r')) {
        *--end = 0;
    }
    return text;
}

int ph_next_pair(char **cursor, char **key, char **value)
{
    while (*cursor && **cursor) {
        char *line = *cursor;
        char *end = SDL_strchr(line, '\n');
        if (end) {
            *end = 0;
            *cursor = end + 1;
        } else {
            *cursor = line + SDL_strlen(line);
        }
        char *equals = SDL_strchr(line, '=');
        if (!equals || line[0] == '#') {
            continue;
        }
        *equals = 0;
        *key = ph_trim(line);
        *value = ph_trim(equals + 1);
        if (**key) {
            return 1;
        }
    }
    return 0;
}

#endif
