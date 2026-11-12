#ifndef PH_CONFIG_H
#define PH_CONFIG_H

#include <SDL3/SDL.h>

#include "ph_files.h"
#include "ph_input.h"
#include "ph_menu.h"
#include "ph_net.h"
#include "ph_settings.h"
#include "ph_touch.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ph_config {
    const struct ph_setting *settings;
    int setting_count;
    struct ph_input *input;
    struct ph_touch *touch;
    struct ph_net *net;
};

int ph_config_take(const struct ph_config *config, const char *key, const char *value);
int ph_config_load(const struct ph_config *config, const char *name);
int ph_config_save(const struct ph_config *config, const char *name);

#ifdef __cplusplus
}
#endif

#endif

#if defined(PH_IMPLEMENTATION) && !defined(PH_CONFIG_IMPLEMENTED)
#define PH_CONFIG_IMPLEMENTED

int ph_config_take(const struct ph_config *config, const char *key, const char *value)
{
    if (ph_settings_take(config->settings, config->setting_count, key, value)) {
        return 1;
    }
    if (ph_menu_take(key, value)) {
        return 1;
    }
    if (config->input && ph_input_take(config->input, key, value)) {
        return 1;
    }
    if (config->touch && ph_touch_take(config->touch, key, value)) {
        return 1;
    }
    return config->net && ph_net_take(config->net, key, value);
}

int ph_config_load(const struct ph_config *config, const char *name)
{
    char path[PH_PATH_SIZE];
    ph_settings_reset(config->settings, config->setting_count);
    ph_menu_reset_flavor();
    char *text = ph_user_path(name, path, sizeof path) ? SDL_LoadFile(path, NULL) : NULL;
    int found = text != NULL;
    char *cursor = text;
    char *key = NULL;
    char *value = NULL;
    while (text && ph_next_pair(&cursor, &key, &value)) {
        ph_config_take(config, key, value);
    }
    SDL_free(text);
    return found;
}

int ph_config_save(const struct ph_config *config, const char *name)
{
    char path[PH_PATH_SIZE];
    SDL_IOStream *file = ph_user_path(name, path, sizeof path) ? SDL_IOFromFile(path, "w") : NULL;
    if (!file) {
        return 0;
    }
    ph_settings_write(file, config->settings, config->setting_count);
    ph_menu_write_config(file);
    if (config->input) {
        ph_input_write(file, config->input);
    }
    if (config->touch) {
        ph_touch_write(file, config->touch);
    }
    if (config->net) {
        ph_net_write(file, config->net);
    }
    return SDL_CloseIO(file);
}

#endif
