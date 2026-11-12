#include "pause.h"
#include "audio.h"
#include "port_crt.h"
#include "port_mod.h"
#include "render.h"
#include "ph_launcher.h"
#include "ph_view.h"
#include <stdio.h>
#include <string.h>
#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

enum {
    SECTION_DISPLAY,
    SECTION_GAME,
    PAUSE_REDRAW_MS = 32,
};

static const char *const pause_sections[] = {
    "Display",
    "Game",
};

static const char *const filter_labels[] = { "Smooth", "Sharp", "CRT" };
static const char *const filter_values[] = { "smooth", "nearest", "crt" };

static const char *const scanline_labels[] = { "Off", "Light", "Medium", "Heavy" };
static const char *const scanline_values[] = { "off", "light", "medium", "heavy" };

static const char *const mask_labels[] = { "Aperture grille", "Slot mask", "Shadow mask", "Off" };
static const char *const mask_values[] = { "aperture", "slot", "shadow", "off" };

static const char *const curve_labels[] = { "Off", "Slight", "Strong" };
static const char *const curve_values[] = { "off", "slight", "strong" };

static const char *const glow_labels[] = { "Off", "Soft", "Strong" };
static const char *const glow_values[] = { "off", "soft", "strong" };

#if TARGET_OS_IPHONE || defined(__ANDROID__)
static const char *const aspect_labels[] = { "4:3", "16:10", "16:9", "21:9", "Fill screen" };
static const char *const aspect_values[] = { "4:3", "16:10", "16:9", "21:9", "fill" };
#else
static const char *const aspect_labels[] = { "4:3", "16:10", "16:9", "21:9" };
static const char *const aspect_values[] = { "4:3", "16:10", "16:9", "21:9" };
#endif

static const char *const bool_values[] = { "false", "true" };
static const char *const mute_values[] = { "true", "false" };

static const char *const touch_labels[] = { "Off", "Smart", "On" };
static const char *const touch_values[] = { "false", "smart", "true" };

enum {
    OPT_FILTER,
    OPT_SCANLINES,
    OPT_CRT_MASK,
    OPT_CRT_CURVE,
    OPT_CRT_GLOW,
    OPT_ASPECT,
    OPT_FULLSCREEN,
    OPT_TOUCH,
    OPT_SOUND,
    OPT_COUNT,
};

static int opt_values[OPT_COUNT];

static struct ph_setting pause_settings[OPT_COUNT] = {
    [OPT_FILTER] = {
        .key = "render.filter",
        .label = "Filter",
        .section = SECTION_DISPLAY,
        .show = PH_SHOW_ALWAYS,
        .value = &opt_values[OPT_FILTER],
        .fallback = 0,
        .choices = filter_labels,
        .choice_count = SDL_arraysize(filter_labels),
    },
    [OPT_SCANLINES] = {
        .key = "render.scanlines",
        .label = "Scanlines",
        .section = SECTION_DISPLAY,
        .show = PH_SHOW_ALWAYS,
        .value = &opt_values[OPT_SCANLINES],
        .fallback = 0,
        .choices = scanline_labels,
        .choice_count = SDL_arraysize(scanline_labels),
    },
    [OPT_CRT_MASK] = {
        .key = "render.crt_mask",
        .label = "CRT mask",
        .section = SECTION_DISPLAY,
        .show = PH_SHOW_HIDDEN,
        .value = &opt_values[OPT_CRT_MASK],
        .fallback = 0,
        .choices = mask_labels,
        .choice_count = SDL_arraysize(mask_labels),
    },
    [OPT_CRT_CURVE] = {
        .key = "render.crt_curvature",
        .label = "CRT curve",
        .section = SECTION_DISPLAY,
        .show = PH_SHOW_HIDDEN,
        .value = &opt_values[OPT_CRT_CURVE],
        .fallback = 0,
        .choices = curve_labels,
        .choice_count = SDL_arraysize(curve_labels),
    },
    [OPT_CRT_GLOW] = {
        .key = "render.crt_glow",
        .label = "CRT glow",
        .section = SECTION_DISPLAY,
        .show = PH_SHOW_HIDDEN,
        .value = &opt_values[OPT_CRT_GLOW],
        .fallback = 0,
        .choices = glow_labels,
        .choice_count = SDL_arraysize(glow_labels),
    },
    [OPT_ASPECT] = {
        .key = "render.aspect",
        .label = "Aspect ratio",
        .section = SECTION_DISPLAY,
        .show = PH_SHOW_ALWAYS,
        .value = &opt_values[OPT_ASPECT],
        .fallback = 0,
        .choices = aspect_labels,
        .choice_count = SDL_arraysize(aspect_labels),
    },
    [OPT_FULLSCREEN] = {
        .key = "render.fullscreen",
        .label = "Fullscreen",
        .section = SECTION_DISPLAY,
        .show = PH_SHOW_DESKTOP,
        .value = &opt_values[OPT_FULLSCREEN],
        .fallback = 0,
        .choices = ph_off_on,
        .choice_count = 2,
    },
    [OPT_TOUCH] = {
        .key = "input.touch",
        .label = "On-screen controls",
        .section = SECTION_GAME,
        .show = PH_SHOW_MOBILE,
        .value = &opt_values[OPT_TOUCH],
        .fallback = 1,
        .choices = touch_labels,
        .choice_count = SDL_arraysize(touch_labels),
    },
    [OPT_SOUND] = {
        .key = "sound.mute",
        .label = "Sound",
        .section = SECTION_GAME,
        .show = PH_SHOW_ALWAYS,
        .value = &opt_values[OPT_SOUND],
        .fallback = 1,
        .choices = ph_off_on,
        .choice_count = 2,
    },
};

static const char *const *const opt_value_tables[OPT_COUNT] = {
    [OPT_FILTER] = filter_values,
    [OPT_SCANLINES] = scanline_values,
    [OPT_CRT_MASK] = mask_values,
    [OPT_CRT_CURVE] = curve_values,
    [OPT_CRT_GLOW] = glow_values,
    [OPT_ASPECT] = aspect_values,
    [OPT_FULLSCREEN] = bool_values,
    [OPT_TOUCH] = touch_values,
    [OPT_SOUND] = mute_values,
};

static struct ph_launcher pause_launcher;
static int pause_initialized;
static int active;
static int chord_guard;
static int pending_action;

static SDL_Surface *pause_surface;
static SDL_Renderer *pause_renderer;
static LauncherPicture pause_picture;
static uint64_t pause_serial;
static int pause_dirty;
static Uint64 last_anim_ms;

static void refresh_crt_visibility(void) {
    int show = opt_values[OPT_FILTER] == 2 ? PH_SHOW_ALWAYS : PH_SHOW_HIDDEN;
    pause_settings[OPT_CRT_MASK].show = show;
    pause_settings[OPT_CRT_CURVE].show = show;
    pause_settings[OPT_CRT_GLOW].show = show;
}

static struct ph_menu_theme chronicle_pause_theme;

static void sync_from_settings(void) {
    RenderDisplay disp = {0};
    render_display(&disp);
    for (int i = 0; i < OPT_COUNT; i++) {
        if (i == OPT_FULLSCREEN) {
            opt_values[i] = disp.fullscreen ? 1 : 0;
            continue;
        }
        if (i == OPT_SOUND) {
            opt_values[i] = audio_is_muted() ? 0 : 1;
            continue;
        }
        if (i == OPT_TOUCH) {
            int mode = port_touch_get_mode();
            opt_values[i] = mode == TOUCH_ON ? 2 : mode == TOUCH_SMART ? 1 : 0;
            continue;
        }
        const char *val = port_setting(pause_settings[i].key);
        if (val && i == OPT_FILTER && !SDL_strcasecmp(val, "sharp")) val = "nearest";
        opt_values[i] = pause_settings[i].fallback;
        for (int c = 0; val && c < pause_settings[i].choice_count; c++) {
            if (!SDL_strcasecmp(opt_value_tables[i][c], val)) {
                opt_values[i] = c;
                break;
            }
        }
    }
    refresh_crt_visibility();
}

static void apply_one_setting(int idx) {
    if (idx < 0 || idx >= OPT_COUNT) return;
    int choice = SDL_clamp(opt_values[idx], 0, pause_settings[idx].choice_count - 1);
    const char *key = pause_settings[idx].key;
    const char *val = opt_value_tables[idx][choice];
    port_launcher_save_setting(key, val);
    if (idx == OPT_FILTER || idx == OPT_SCANLINES || idx == OPT_CRT_MASK ||
        idx == OPT_CRT_CURVE || idx == OPT_CRT_GLOW) {
        render_apply_crt_settings();
    } else if (idx == OPT_ASPECT) {
        int num = 0, den = 0;
        if (!strcmp(val, "fill")) render_fill_screen();
        else if (sscanf(val, "%d:%d", &num, &den) == 2) render_set_aspect(num, den);
        render_redraw();
    } else if (idx == OPT_FULLSCREEN) {
        render_set_window(0, 0, !strcmp(val, "true"));
        render_redraw();
    } else if (idx == OPT_TOUCH) {
        port_touch_set_mode(!strcmp(val, "smart") ? TOUCH_SMART : !strcmp(val, "true") ? TOUCH_ON : TOUCH_OFF);
        render_redraw();
    } else if (idx == OPT_SOUND) {
        audio_set_muted(!strcmp(val, "true"));
    }
}

static void ensure_pause_canvas(int pw, int ph) {
    SDL_Window *win = render_window();
    if ((pw <= 0 || ph <= 0) && win) {
        SDL_GetWindowSizeInPixels(win, &pw, &ph);
    }
    if (pw <= 0) pw = 640;
    if (ph <= 0) ph = 480;
    if (!pause_surface || !pause_renderer || pause_surface->w != pw || pause_surface->h != ph) {
        if (pause_renderer) {
            ph_view_set_window(pause_renderer, NULL);
            SDL_DestroyRenderer(pause_renderer);
            pause_renderer = NULL;
        }
        if (pause_surface) {
            SDL_DestroySurface(pause_surface);
            pause_surface = NULL;
        }
        pause_surface = SDL_CreateSurface(pw, ph, SDL_PIXELFORMAT_RGBA32);
        if (pause_surface) {
            pause_renderer = SDL_CreateSoftwareRenderer(pause_surface);
            if (pause_renderer) {
                SDL_SetRenderDrawBlendMode(pause_renderer, SDL_BLENDMODE_BLEND);
            }
        }
        pause_dirty = 1;
    }
    if (pause_renderer) {
        ph_view_set_window(pause_renderer, win);
    }
}

static void ensure_pause_init(void) {
    if (pause_initialized) {
        pause_launcher.window = render_window();
        return;
    }
    chronicle_pause_theme = ph_theme_make(0x1B2848, 0x141E38, 0x0D1324, 0xF2E6CE, 0xE8B948);
    ph_launcher_init(&pause_launcher, render_window());
    pause_launcher.menu.theme = &chronicle_pause_theme;
    pause_launcher.settings = pause_settings;
    pause_launcher.setting_count = OPT_COUNT;
    pause_launcher.sections = pause_sections;
    pause_launcher.section_count = SDL_arraysize(pause_sections);
    pause_launcher.slots = 1;
    pause_launcher.no_reset = 1;
    pause_launcher.quit_from_pause = 1;
    pause_launcher.hide_rom = 1;
    pause_initialized = 1;
}

static int any_chord_button_held(void) {
    int count = 0;
    SDL_JoystickID *ids = SDL_GetGamepads(&count);
    int held = 0;
    for (int i = 0; ids && i < count; i++) {
        SDL_Gamepad *pad = SDL_GetGamepadFromID(ids[i]);
        if (!pad) pad = SDL_OpenGamepad(ids[i]);
        if (pad && (SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_START) ||
                    SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_BACK))) {
            held = 1;
            break;
        }
    }
    SDL_free(ids);
    return held;
}

static void handle_launch_result(struct ph_launch res) {
    refresh_crt_visibility();
    switch (res.kind) {
    case PH_LAUNCH_RESUME:
        pending_action = PAUSE_RESUME;
        break;
    case PH_LAUNCH_SAVE_STATE:
        pending_action = PAUSE_SAVE_STATE;
        break;
    case PH_LAUNCH_LOAD_STATE:
        pending_action = PAUSE_LOAD_STATE;
        break;
    case PH_LAUNCH_QUIT:
    case PH_LAUNCH_END:
        pending_action = PAUSE_QUIT;
        break;
    case PH_LAUNCH_SETTING:
        if (res.value == PH_SETTINGS_ALL) {
            for (int i = 0; i < OPT_COUNT; i++) apply_one_setting(i);
        } else {
            apply_one_setting(res.value);
        }
        break;
    case PH_LAUNCH_SAVE_CONFIG:
        break;
    default:
        break;
    }
}

void port_pause_open(int opened_by_chord) {
    ensure_pause_init();
    sync_from_settings();
    ensure_pause_canvas(0, 0);
    ph_launcher_open(&pause_launcher, PH_PAGE_PAUSE);
    active = 1;
    chord_guard = opened_by_chord;
    pending_action = PAUSE_NONE;
    pause_dirty = 1;
    last_anim_ms = SDL_GetTicks();
}

void port_pause_close(void) {
    if (pause_initialized) ph_launcher_close(&pause_launcher);
    active = 0;
    chord_guard = 0;
    pending_action = PAUSE_NONE;
}

int port_pause_active(void) { return active; }

void port_pause_set_status(const char *message) {
    ensure_pause_init();
    ph_launcher_notice(&pause_launcher, message ? message : "");
    pause_dirty = 1;
    render_redraw();
}

int port_pause_event(const SDL_Event *event) {
    if (!active || !event) return PAUSE_NONE;
    ensure_pause_init();
    ensure_pause_canvas(0, 0);

    if (event->type == SDL_EVENT_GAMEPAD_ADDED) SDL_OpenGamepad(event->gdevice.which);
    if (event->type == SDL_EVENT_GAMEPAD_BUTTON_UP) {
        if (chord_guard && !any_chord_button_held()) chord_guard = 0;
    }
    if (event->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN && chord_guard &&
        (event->gbutton.button == SDL_GAMEPAD_BUTTON_START || event->gbutton.button == SDL_GAMEPAD_BUTTON_BACK)) {
        return pending_action;
    }

    if (pause_renderer) {
        struct ph_launch res = ph_launcher_event(&pause_launcher, pause_renderer, event);
        handle_launch_result(res);
        pause_dirty = 1;
        render_redraw();
    }
    return pending_action;
}

int port_pause_tick(void) {
    if (!active) return PAUSE_NONE;
    if (chord_guard && !any_chord_button_held()) chord_guard = 0;
    struct ph_launch res = ph_launcher_update(&pause_launcher);
    if (res.kind != PH_LAUNCH_NONE) {
        handle_launch_result(res);
        pause_dirty = 1;
        render_redraw();
    }
    Uint64 now = SDL_GetTicks();
    if (now - last_anim_ms >= PAUSE_REDRAW_MS) {
        last_anim_ms = now;
        pause_dirty = 1;
        render_redraw();
    }
    return pending_action;
}

const LauncherPicture *port_pause_atlas(int pixel_width, int pixel_height) {
    if (!active || pixel_width <= 0 || pixel_height <= 0) return NULL;
    ensure_pause_init();
    ensure_pause_canvas(pixel_width, pixel_height);
    if (!pause_surface || !pause_renderer) return NULL;
    if (pause_dirty) {
        SDL_SetRenderDrawColor(pause_renderer, 0, 0, 0, 0);
        SDL_RenderClear(pause_renderer);
        ph_launcher_draw(&pause_launcher, pause_renderer);
        SDL_RenderPresent(pause_renderer);
        pause_picture = (LauncherPicture){pause_surface->w, pause_surface->h, (unsigned char *)pause_surface->pixels};
        pause_serial++;
        pause_dirty = 0;
    }
    return &pause_picture;
}

uint64_t port_pause_atlas_serial(void) { return pause_serial; }

int port_pause_shapes(TouchShape *shapes, int limit, int pixel_width, int pixel_height, SDL_Rect *clip, int *clip_start) {
    if (!active || limit < 1 || pixel_width <= 0 || pixel_height <= 0) return 0;
    const LauncherPicture *pic = port_pause_atlas(pixel_width, pixel_height);
    if (!pic || !pic->pixels) return 0;
    shapes[0] = (TouchShape){
        pixel_width * 0.5f,
        pixel_height * 0.5f,
        pixel_width * 0.5f,
        pixel_height * 0.5f,
        0.0f,
        1.0f,
        1.0f,
        1.0f,
        1.0f,
        0.0f,
        0.0f,
        TOUCH_RGBA,
        0.0f,
        0.0f,
        1.0f,
        1.0f,
    };
    if (clip) *clip = (SDL_Rect){0, 0, pixel_width, pixel_height};
    if (clip_start) *clip_start = 1;
    return 1;
}
