#include "launcher.h"
#include "cdvd.h"
#include "executable.h"
#include "icon.h"
#include "memory_card.h"
#include "port_mod.h"
#include "port_overrides.h"
#include "ph_draw.h"
#include "ph_launcher.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

#if TARGET_OS_IPHONE
#include "ios_files.h"
enum { DESKTOP = 0 };
#define WINDOW_FLAGS (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_HIGH_PIXEL_DENSITY)
#define MODS_HINT_TOP "Put built mods (.dc2pak files) in the mods folder:"
#define MODS_HINT_BOTTOM "Files, On My iPhone, DC2, mods."
#define MODS_ACTION "Open mods folder"
#elif defined(__ANDROID__)
#include "android_files.h"
enum { DESKTOP = 0 };
#define WINDOW_FLAGS (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_HIGH_PIXEL_DENSITY)
#define MODS_HINT_TOP "Add built mods (.dc2pak files) from your files,"
#define MODS_HINT_BOTTOM "or copy them to Android/data/.../files/mods."
#define MODS_ACTION "Add a mod"
#else
enum { DESKTOP = 1 };
#define WINDOW_FLAGS (SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY)
#define MODS_HINT_TOP "Built mods (.dc2pak files) go in the mods folder"
#define MODS_HINT_BOTTOM "next to the game. Drop one here to add it."
#define MODS_ACTION "Open mods folder"
#endif

enum {
    CANVAS_WIDTH = 960,
    CANVAS_HEIGHT = 620,
    LARGEST_HEIGHT = 860,
    PATH_SIZE = 1024,
    LINE_SIZE = 256,
    MOD_LIMIT = 32,
    ENABLED_SIZE = 2048,
    OPTION_CHOICES = 16,
};

enum { PICK_NONE, PICK_FILE, PICK_SAVES };
enum { DISC_NONE, DISC_GOOD, DISC_WRONG };

enum {
    PAGE_MODS = PH_PAGE_USER,
    PAGE_SAVES = PH_PAGE_USER + 1,
};

enum {
    ACTION_OPEN_MODS = 1,
    ACTION_OPEN_SAVES = 2,
    ACTION_MODS_FOLDER = 3,
    ACTION_CHOOSE_SAVES = 4,
    ACTION_RESET_SAVES = 5,
    ACTION_MOD_BASE = 100,
};

enum {
    SEC_DISPLAY,
    SEC_MOTION,
    SEC_CONTROLS,
    SEC_GAME,
    SEC_STARTUP,
    SEC_COUNT,
};

static const char *const section_names[SEC_COUNT] = {
    "Display",
    "Motion",
    "Controls",
    "Game",
    "Startup",
};

typedef struct {
    int section;
    const char *label;
    const char *setting;
    int count;
    const char *names[OPTION_CHOICES];
    const char *values[OPTION_CHOICES];
    int fallback;
    int debug_tools;
    int crt_only;
} LauncherOptionDef;

#if TARGET_OS_IPHONE || defined(__ANDROID__)
static const LauncherOptionDef option_defs[] = {
    {SEC_DISPLAY, "Aspect ratio", "render.aspect", 5,
     {"4:3", "16:10", "16:9", "21:9", "Fill screen"},
     {"4:3", "16:10", "16:9", "21:9", "fill"}, 0, 0, 0},
    {SEC_DISPLAY, "Render scale", "render.scale", 5,
     {"Match screen", "1x", "2x", "3x", "4x"},
     {"auto", "1", "2", "3", "4"}, 1, 0, 0},
    {SEC_DISPLAY, "Anti-aliasing", "render.msaa", 4,
     {"Off", "2x", "4x", "8x"},
     {"1", "2", "4", "8"}, 0, 0, 0},
    {SEC_DISPLAY, "Filter", "render.filter", 3,
     {"Smooth", "Sharp", "CRT"},
     {"smooth", "nearest", "crt"}, 0, 0, 0},
    {SEC_DISPLAY, "Scanlines", "render.scanlines", 4,
     {"Off", "Light", "Medium", "Heavy"},
     {"off", "light", "medium", "heavy"}, 0, 0, 0},
    {SEC_DISPLAY, "CRT mask", "render.crt_mask", 4,
     {"Aperture grille", "Slot mask", "Shadow mask", "Off"},
     {"aperture", "slot", "shadow", "off"}, 0, 0, 1},
    {SEC_DISPLAY, "CRT curve", "render.crt_curvature", 3,
     {"Off", "Slight", "Strong"},
     {"off", "slight", "strong"}, 0, 0, 1},
    {SEC_DISPLAY, "CRT glow", "render.crt_glow", 3,
     {"Off", "Soft", "Strong"},
     {"off", "soft", "strong"}, 0, 0, 1},
    {SEC_MOTION, "High frame rate", "render.smooth", 4,
     {"Off", "Match display", "60 Hz", "120 Hz"},
     {"false", "display", "60", "120"}, 0, 0, 0},
    {SEC_CONTROLS, "On-screen controls", "input.touch", 3,
     {"Off", "Smart", "On"},
     {"false", "smart", "true"}, 1, 0, 0},
    {SEC_CONTROLS, "Haptics", "input.haptics", 2,
     {"Off", "On"}, {"false", "true"}, 1, 0, 0},
    {SEC_CONTROLS, "Save state buttons", "input.states", 2,
     {"Off", "On"}, {"false", "true"}, 1, 0, 0},
    {SEC_GAME, "Sound", "sound.mute", 2,
     {"Off", "On"}, {"true", "false"}, 1, 0, 0},
    {SEC_GAME, "In-Game Debug Menu", "debug.menus", 2,
     {"Off", "On"}, {"false", "true"}, 0, 0, 0},
    {SEC_STARTUP, "Language", "boot.language", 6,
     {"Ask", "English", "Francais", "Deutsch", "Italiano", "Espanol"},
     {"ask", "1", "2", "3", "4", "5"}, 0, 0, 0},
    {SEC_STARTUP, "Skip intro", "boot.skip_intro", 2,
     {"Off", "On"}, {"false", "true"}, 0, 0, 0},
    {SEC_STARTUP, "Load save", "boot.save", 14,
     {"Off", "Slot 1", "Slot 2", "Slot 3", "Slot 4", "Slot 5", "Slot 6", "Slot 7", "Slot 8", "Slot 9", "Slot 10",
      "Slot 11", "Slot 12", "Slot 13"},
     {"off", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13"}, 0, 0, 0},
};
#else
static const LauncherOptionDef option_defs[] = {
    {SEC_DISPLAY, "Window size", "render.window", 8,
     {"640 x 480", "960 x 720", "1280 x 720", "1280 x 960", "1600 x 900", "1920 x 1080", "2560 x 1440", "3840 x 2160"},
     {"640x480", "960x720", "1280x720", "1280x960", "1600x900", "1920x1080", "2560x1440", "3840x2160"}, 0, 0, 0},
    {SEC_DISPLAY, "Fullscreen", "render.fullscreen", 2,
     {"Off", "On"}, {"false", "true"}, 0, 0, 0},
    {SEC_DISPLAY, "Aspect ratio", "render.aspect", 4,
     {"4:3", "16:10", "16:9", "21:9"},
     {"4:3", "16:10", "16:9", "21:9"}, 0, 0, 0},
    {SEC_DISPLAY, "Render scale", "render.scale", 5,
     {"Match window", "1x", "2x", "3x", "4x"},
     {"auto", "1", "2", "3", "4"}, 1, 0, 0},
    {SEC_DISPLAY, "Anti-aliasing", "render.msaa", 4,
     {"Off", "2x", "4x", "8x"},
     {"1", "2", "4", "8"}, 0, 0, 0},
    {SEC_DISPLAY, "Filter", "render.filter", 3,
     {"Smooth", "Sharp", "CRT"},
     {"smooth", "nearest", "crt"}, 0, 0, 0},
    {SEC_DISPLAY, "Scanlines", "render.scanlines", 4,
     {"Off", "Light", "Medium", "Heavy"},
     {"off", "light", "medium", "heavy"}, 0, 0, 0},
    {SEC_DISPLAY, "CRT mask", "render.crt_mask", 4,
     {"Aperture grille", "Slot mask", "Shadow mask", "Off"},
     {"aperture", "slot", "shadow", "off"}, 0, 0, 1},
    {SEC_DISPLAY, "CRT curve", "render.crt_curvature", 3,
     {"Off", "Slight", "Strong"},
     {"off", "slight", "strong"}, 0, 0, 1},
    {SEC_DISPLAY, "CRT glow", "render.crt_glow", 3,
     {"Off", "Soft", "Strong"},
     {"off", "soft", "strong"}, 0, 0, 1},
    {SEC_DISPLAY, "Vertical sync", "render.vsync", 2,
     {"Off", "On"}, {"false", "true"}, 0, 0, 0},
    {SEC_MOTION, "High frame rate", "render.smooth", 8,
     {"Off", "Match display", "60 Hz", "120 Hz", "144 Hz", "165 Hz", "240 Hz", "Unlimited"},
     {"false", "display", "60", "120", "144", "165", "240", "1000"}, 0, 0, 0},
    {SEC_GAME, "Sound", "sound.mute", 2,
     {"Off", "On"}, {"true", "false"}, 1, 0, 0},
    {SEC_GAME, "In-Game Debug Menu", "debug.menus", 2,
     {"Off", "On"}, {"false", "true"}, 0, 0, 0},
    {SEC_GAME, "Port Developer Panel", "debug.window", 2,
     {"Hidden", "Shown"}, {"false", "true"}, 0, 1, 0},
    {SEC_STARTUP, "Language", "boot.language", 6,
     {"Ask", "English", "Francais", "Deutsch", "Italiano", "Espanol"},
     {"ask", "1", "2", "3", "4", "5"}, 0, 0, 0},
    {SEC_STARTUP, "Skip intro", "boot.skip_intro", 2,
     {"Off", "On"}, {"false", "true"}, 0, 0, 0},
    {SEC_STARTUP, "Load save", "boot.save", 14,
     {"Off", "Slot 1", "Slot 2", "Slot 3", "Slot 4", "Slot 5", "Slot 6", "Slot 7", "Slot 8", "Slot 9", "Slot 10",
      "Slot 11", "Slot 12", "Slot 13"},
     {"off", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13"}, 0, 0, 0},
};
#endif

enum { OPTION_COUNT = SDL_arraysize(option_defs) };

static const unsigned char backdrop_png[] = {
#embed "../launcher/backdrop.png"
};

static const struct ph_launcher_item custom_items[] = {
    { PH_PAGE_MAIN, ACTION_OPEN_MODS, "Mods" },
    { PH_PAGE_MAIN, ACTION_OPEN_SAVES, "Saves" },
    { PH_PAGE_SETTINGS, ACTION_OPEN_SAVES, "Saves folder" },
};

static int option_values[OPTION_COUNT];
static struct ph_setting ph_options[OPTION_COUNT];
static struct ph_launcher ui;

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *backdrop;
static SDL_Texture *disc_logo;

static int disc_state;
static char disc_path[PATH_SIZE];
static char disc_name[PATH_SIZE];
static char settings_path[PATH_SIZE];
static char picked_path[PATH_SIZE];
static SDL_AtomicInt picked;
static int choosing;
static char saves_path[PATH_SIZE];
static char env_saves[PATH_SIZE];
static char saves_name[LINE_SIZE];

static int mod_count;
static int mod_on[MOD_LIMIT];

static struct ph_menu_theme chronicle_theme;

static int filter_is_crt(void) {
    for (int i = 0; i < OPTION_COUNT; i++) {
        if (!strcmp(option_defs[i].setting, "render.filter")) {
            int c = SDL_clamp(option_values[i], 0, option_defs[i].count - 1);
            return !strcmp(option_defs[i].values[c], "crt");
        }
    }
    return 0;
}

static void refresh_option_visibility(void) {
    int crt = filter_is_crt();
    int has_debug = port_mod_loaded("dc2tk.debug-tools");
    for (int i = 0; i < OPTION_COUNT; i++) {
        int visible = 1;
        if (option_defs[i].debug_tools && !has_debug) visible = 0;
        if (option_defs[i].crt_only && !crt) visible = 0;
        ph_options[i].show = visible ? PH_SHOW_ALWAYS : PH_SHOW_HIDDEN;
    }
}

static const char *base_name(const char *path) {
    const char *slash = strrchr(path, '/');
    const char *backslash = strrchr(path, '\\');
    if (backslash > slash) slash = backslash;
    return slash ? slash + 1 : path;
}

static void file_name(const char *path, char *name, size_t size) {
#ifdef __ANDROID__
    if (!strncmp(path, "content://", 10)) {
        port_android_name(path, name, size);
        return;
    }
#endif
    snprintf(name, size, "%s", base_name(path));
}

#if !TARGET_OS_IPHONE && !defined(__ANDROID__)
static void trim_path(char *path) {
    size_t len = strlen(path);
    while (len > 1 && (path[len - 1] == '/' || path[len - 1] == '\\')) {
        if (len == 3 && path[1] == ':') break;
        path[--len] = 0;
    }
}
#endif

static void refresh_saves_name(void) {
#if TARGET_OS_IPHONE
    port_ios_saves_name(saves_name, sizeof saves_name);
#elif defined(__ANDROID__)
    saves_name[0] = 0;
    if (saves_path[0]) file_name(saves_path, saves_name, sizeof saves_name);
    if (!saves_name[0]) snprintf(saves_name, sizeof saves_name, "On this device");
#else
    const char *active = env_saves[0] ? env_saves : saves_path[0] ? saves_path : NULL;
    if (active) {
        char clean[PATH_SIZE];
        snprintf(clean, sizeof clean, "%s", active);
        trim_path(clean);
        const char *leaf = base_name(clean);
        snprintf(saves_name, sizeof saves_name, "%s",
                 !strcmp(leaf, "com~apple~CloudDocs") ? "iCloud Drive" : *leaf ? leaf : clean);
    } else {
        snprintf(saves_name, sizeof saves_name, "%s",
                 strstr(port_memory_card_default_root(), "com~apple~CloudDocs") ? "iCloud Drive" : "Default");
    }
#endif
}

static void build_ph_options(void) {
    for (int i = 0; i < OPTION_COUNT; i++) {
        ph_options[i] = (struct ph_setting){
            .key = option_defs[i].setting,
            .label = option_defs[i].label,
            .section = option_defs[i].section,
            .show = PH_SHOW_ALWAYS,
            .value = &option_values[i],
            .fallback = option_defs[i].fallback,
            .choices = option_defs[i].names,
            .choice_count = option_defs[i].count,
        };
        option_values[i] = option_defs[i].fallback;
    }
}

static void read_options(void) {
    build_ph_options();
    for (int i = 0; i < OPTION_COUNT; i++) {
        const char *value = port_setting(option_defs[i].setting);
        if (value && !strcmp(option_defs[i].setting, "render.filter") && !SDL_strcasecmp(value, "sharp")) {
            value = "nearest";
        }
        for (int c = 0; value && c < option_defs[i].count; c++) {
            if (!SDL_strcasecmp(option_defs[i].values[c], value)) {
                option_values[i] = c;
                break;
            }
        }
    }
    refresh_option_visibility();
    const char *disc = port_setting("disc");
    const char *saves = port_setting("saves");
    const char *override_saves = SDL_getenv("DC2_SAVE_ROOT");
    snprintf(disc_path, sizeof disc_path, "%s", disc ? disc : "");
    snprintf(saves_path, sizeof saves_path, "%s", saves ? saves : "");
    snprintf(env_saves, sizeof env_saves, "%s", override_saves ? override_saves : "");
#if TARGET_OS_IPHONE
    port_ios_disc(disc_path, sizeof disc_path);
#endif
    refresh_saves_name();
}

static void save_options(void) {
    FILE *file = fopen(settings_path, "w");
    for (int i = 0; i < OPTION_COUNT; i++) {
        int c = SDL_clamp(option_values[i], 0, option_defs[i].count - 1);
        const char *val = option_defs[i].values[c];
        port_setting_set(option_defs[i].setting, val);
        if (file) fprintf(file, "%s=%s\n", option_defs[i].setting, val);
    }
    char enabled[ENABLED_SIZE] = "";
    for (int i = 0; i < mod_count; i++) {
        size_t used = strlen(enabled);
        if (mod_on[i]) {
            snprintf(enabled + used, sizeof enabled - used, "%s%s", used ? "," : "", port_pak_text(i, "id"));
        }
    }
    port_setting_set("disc", disc_path);
    port_setting_set("saves", saves_path);
    port_setting_set("mods.enabled", enabled);
    if (file) {
        fprintf(file, "disc=%s\nsaves=%s\nmods.enabled=%s\n",
                disc_path, saves_path, enabled);
        fclose(file);
    } else if (settings_path[0]) {
        fprintf(stderr, "launcher: cannot save %s\n", settings_path);
    }
}

static uint16_t dc_le16(const uint8_t *p) {
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t dc_le32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int archive_path_eq(const char *a, const char *b) {
    while (*a == '/' || *a == '\\') a++;
    while (*b == '/' || *b == '\\') b++;
    while (*a && *b) {
        char ca = *a == '\\' ? '/' : (char)SDL_tolower((unsigned char)*a);
        char cb = *b == '\\' ? '/' : (char)SDL_tolower((unsigned char)*b);
        if (ca != cb) return 0;
        a++;
        b++;
    }
    return *a == 0 && *b == 0;
}

static uint8_t *read_disc_asset(const char *rel_path, size_t *out_size) {
    *out_size = 0;
    struct sceCdlFILE direct;
    if (port_cdvd_find(rel_path, &direct) && !(direct.flag & 0x02) &&
        direct.size > 0 && direct.size <= 16u * 1024u * 1024u) {
        uint8_t *buf = (uint8_t *)malloc(direct.size);
        if (buf && port_cdvd_read_file(&direct, 0, buf, direct.size)) {
            *out_size = direct.size;
            return buf;
        }
        free(buf);
    }

    struct sceCdlFILE data_file, hd4_file;
    if (!port_cdvd_find("DATA.DAT", &data_file) || !port_cdvd_find("DATA.HD4", &hd4_file)) return NULL;
    if (hd4_file.size < 12 || hd4_file.size > 0x100000u) return NULL;

    uint8_t *hd4 = (uint8_t *)malloc(hd4_file.size);
    if (!hd4) return NULL;
    if (!port_cdvd_read_file(&hd4_file, 0, hd4, hd4_file.size)) {
        free(hd4);
        return NULL;
    }

    uint32_t names_off = dc_le32(hd4);
    if (names_off < 12 || names_off > hd4_file.size || (names_off % 12) != 0) {
        free(hd4);
        return NULL;
    }

    uint32_t count = names_off / 12;
    for (uint32_t i = 0; i < count; i++) {
        const uint8_t *rec = hd4 + (size_t)i * 12;
        uint32_t name_off = dc_le32(rec);
        uint32_t size = dc_le32(rec + 4);
        uint32_t sector = dc_le32(rec + 8);
        if (name_off < names_off || name_off >= hd4_file.size) continue;
        if (!memchr(hd4 + name_off, 0, hd4_file.size - name_off)) continue;
        if (!archive_path_eq((const char *)(hd4 + name_off), rel_path)) continue;
        if (size == 0 || size > 16u * 1024u * 1024u) break;
        uint8_t *buf = (uint8_t *)malloc(size);
        if (buf && port_cdvd_read_file(&data_file, (uint64_t)sector * CDVD_SECTOR_SIZE, buf, size)) {
            free(hd4);
            *out_size = size;
            return buf;
        }
        free(buf);
        break;
    }

    free(hd4);
    return NULL;
}

static uint32_t gs_addr32(uint32_t pages_wide, uint32_t x, uint32_t y) {
    static const uint16_t col_x[64] = {
        0, 4, 16, 20, 32, 36, 48, 52, 256, 260, 272, 276, 288, 292, 304, 308,
        1024, 1028, 1040, 1044, 1056, 1060, 1072, 1076, 1280, 1284, 1296, 1300, 1312, 1316, 1328, 1332,
        4096, 4100, 4112, 4116, 4128, 4132, 4144, 4148, 4352, 4356, 4368, 4372, 4384, 4388, 4400, 4404,
        5120, 5124, 5136, 5140, 5152, 5156, 5168, 5172, 5376, 5380, 5392, 5396, 5408, 5412, 5424, 5428
    };
    static const uint16_t col_y[32] = {
        0, 8, 64, 72, 128, 136, 192, 200, 512, 520, 576, 584, 640, 648, 704, 712,
        2048, 2056, 2112, 2120, 2176, 2184, 2240, 2248, 2560, 2568, 2624, 2632, 2688, 2696, 2752, 2760
    };
    uint32_t page = ((x >> 6) + (y >> 5) * pages_wide) * 32u;
    return (page << 8) + col_x[x & 63u] + col_y[y & 31u];
}

static uint32_t gs_addr8(uint32_t pages_wide, uint32_t x, uint32_t y) {
    uint32_t bx = (x >> 4) & 7u;
    uint32_t by = (y >> 4) & 3u;
    uint32_t block = (bx & 1u) | ((by & 1u) << 1) | ((bx & 2u) << 1) | ((by & 2u) << 2) | ((bx & 4u) << 2);
    block += ((x >> 7) + (y >> 6) * pages_wide) * 32u;
    uint32_t column = ((y & 2u) >> 1) | ((x & 8u) >> 2) | ((x & 1u) << 2) | ((y & 1u) << 3) |
                      ((x & 2u) << 3) | (((x ^ y ^ (y << 1)) & 4u) << 3) | ((y & 12u) << 4);
    return (block << 8) + column;
}

static uint8_t *unswizzle_ps2_8bpp(const uint8_t *src, size_t src_len, int width, int height) {
    uint32_t pages_wide = (uint32_t)(width + 127) / 128u;
    uint32_t pages_high = (uint32_t)(height + 63) / 64u;
    size_t word_capacity = (size_t)pages_wide * (size_t)pages_high * (8192u / 4u);
    int *word_map = (int *)malloc(word_capacity * sizeof(int));
    uint8_t *linear = (uint8_t *)calloc((size_t)width * (size_t)height, 1);
    if (!word_map || !linear) {
        free(word_map);
        free(linear);
        return NULL;
    }
    for (size_t i = 0; i < word_capacity; i++) word_map[i] = -1;

    int words_wide = width / 2;
    int words_high = height / 2;
    for (int y = 0; y < words_high; y++) {
        for (int x = 0; x < words_wide; x++) {
            size_t word = gs_addr32(pages_wide, (uint32_t)x, (uint32_t)y) / 4u;
            if (word < word_capacity) word_map[word] = y * words_wide + x;
        }
    }

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            uint32_t addr = gs_addr8(pages_wide, (uint32_t)x, (uint32_t)y);
            size_t word = addr / 4u;
            if (word >= word_capacity || word_map[word] < 0) continue;
            size_t byte_idx = (size_t)word_map[word] * 4u + (addr & 3u);
            if (byte_idx < src_len) linear[(size_t)y * (size_t)width + (size_t)x] = src[byte_idx];
        }
    }

    free(word_map);
    return linear;
}

static uint8_t ps2_alpha_to_u8(uint8_t a) {
    return a >= 0x80u ? 255u : (uint8_t)(a * 2u);
}

static void read_tim2_clut_rgba(const uint8_t *clut, size_t clut_bytes, uint8_t clut_type, int index, uint8_t out[4]) {
    out[0] = out[1] = out[2] = out[3] = 0;
    int fmt = clut_type & 0x3F;
    if (fmt == 3) {
        size_t off = (size_t)index * 4u;
        if (off + 4u > clut_bytes) return;
        out[0] = clut[off + 0];
        out[1] = clut[off + 1];
        out[2] = clut[off + 2];
        out[3] = ps2_alpha_to_u8(clut[off + 3]);
    } else if (fmt == 2) {
        size_t off = (size_t)index * 3u;
        if (off + 3u > clut_bytes) return;
        out[0] = clut[off + 0];
        out[1] = clut[off + 1];
        out[2] = clut[off + 2];
        out[3] = 255u;
    } else if (fmt == 1) {
        size_t off = (size_t)index * 2u;
        if (off + 2u > clut_bytes) return;
        uint16_t raw = dc_le16(clut + off);
        out[0] = (uint8_t)(((raw & 31u) * 255u) / 31u);
        out[1] = (uint8_t)((((raw >> 5) & 31u) * 255u) / 31u);
        out[2] = (uint8_t)((((raw >> 10) & 31u) * 255u) / 31u);
        out[3] = (raw & 0x8000u) ? 255u : 0u;
    }
}

static SDL_Surface *decode_chro_from_img(const uint8_t *img, size_t img_size) {
    if (!img || img_size < 0x40) return NULL;
    size_t tm2_off = 0;
    int swizzled = 0;

    if (memcmp(img, "IMG", 3) == 0 || memcmp(img, "IM2", 3) == 0) {
        uint32_t num = dc_le32(img + 4);
        swizzled = (memcmp(img, "IM2", 3) == 0);
        if (num > 4096 || 0x10u + (size_t)num * 0x30u > img_size) return NULL;
        for (uint32_t i = 0; i < num; i++) {
            const uint8_t *ent = img + 0x10u + (size_t)i * 0x30u;
            char name[0x21];
            memcpy(name, ent, 0x20);
            name[0x20] = 0;
            if (SDL_strcasecmp(name, "chro") == 0) {
                int32_t off = (int32_t)dc_le32(ent + 0x20);
                if (off >= 0x10 && (size_t)off < img_size) tm2_off = (size_t)off;
                break;
            }
        }
    } else if (memcmp(img, "IM3", 3) == 0) {
        uint32_t num3 = dc_le32(img + 8);
        if (num3 > 4096 || 0x10u + (size_t)num3 * 0x40u > img_size) return NULL;
        for (uint32_t i = 0; i < num3; i++) {
            const uint8_t *ent = img + 0x10u + (size_t)i * 0x40u;
            char name[0x21];
            memcpy(name, ent, 0x20);
            name[0x20] = 0;
            int16_t no_image = (int16_t)dc_le16(ent + 0x30);
            if (name[0] != '#' && !no_image && SDL_strcasecmp(name, "chro") == 0) {
                int32_t off = (int32_t)dc_le32(ent + 0x24);
                swizzled = (int32_t)dc_le32(ent + 0x28) != 0;
                if (off >= 0x10 && (size_t)off < img_size) tm2_off = (size_t)off;
                break;
            }
        }
    }

    if (!tm2_off || img_size - tm2_off < 0x40) return NULL;
    const uint8_t *tm2 = img + tm2_off;
    size_t tm2_rem = img_size - tm2_off;
    if (memcmp(tm2, "TIM2", 4) != 0) return NULL;
    size_t hdr_off = (tm2[5] == 1) ? 0x80u : 0x10u;
    if (tm2_rem < hdr_off + 0x30u) return NULL;

    const uint8_t *pic = tm2 + hdr_off;
    size_t pic_rem = tm2_rem - hdr_off;
    uint32_t clut_size = dc_le32(pic + 0x04);
    uint32_t image_size = dc_le32(pic + 0x08);
    uint16_t header_size = dc_le16(pic + 0x0C);
    uint8_t mipmap_count = pic[0x11];
    uint8_t clut_type = pic[0x12];
    uint8_t image_type = pic[0x13];
    int width = dc_le16(pic + 0x14);
    int height = dc_le16(pic + 0x16);

    if (width <= 0 || width > 1024 || height <= 0 || height > 1024) return NULL;
    if (header_size < 0x30u || (size_t)header_size + (size_t)image_size > pic_rem) return NULL;
    if ((image_type == 4 || image_type == 5) && (size_t)header_size + (size_t)image_size + (size_t)clut_size > pic_rem) return NULL;

    const uint8_t *src_pixels = pic + header_size;
    const uint8_t *clut = src_pixels + image_size;
    uint8_t *unswizzled = NULL;
    if (swizzled && image_type == 5 && mipmap_count <= 1) {
        unswizzled = unswizzle_ps2_8bpp(src_pixels, image_size, width, height);
        if (!unswizzled) return NULL;
        src_pixels = unswizzled;
    }

    int quad_w = SDL_min(width, 0x200);
    int quad_h = SDL_min(height, 0x1A0);
    uint8_t *rgba = (uint8_t *)calloc((size_t)quad_w * (size_t)quad_h * 4u, 1);
    if (!rgba) {
        free(unswizzled);
        return NULL;
    }

    int min_x = quad_w, min_y = quad_h, max_x = -1, max_y = -1;
    for (int y = 0; y < quad_h; y++) {
        for (int x = 0; x < quad_w; x++) {
            size_t i = (size_t)y * (size_t)width + (size_t)x;
            uint8_t c[4] = { 0, 0, 0, 0 };
            if (image_type == 5) {
                if (i < image_size || unswizzled) {
                    int idx = src_pixels[i];
                    int cidx = (clut_type & 0x80u) ? idx : ((idx & 0xE7) | ((idx & 0x08) << 1) | ((idx & 0x10) >> 1));
                    read_tim2_clut_rgba(clut, clut_size, clut_type, cidx, c);
                }
            } else if (image_type == 4) {
                if (i / 2u < image_size) {
                    int idx = (src_pixels[i / 2u] >> ((i & 1u) * 4u)) & 0x0F;
                    read_tim2_clut_rgba(clut, clut_size, clut_type, idx, c);
                }
            } else if (image_type == 3) {
                if (i * 4u + 4u <= image_size) {
                    c[0] = src_pixels[i * 4u + 0];
                    c[1] = src_pixels[i * 4u + 1];
                    c[2] = src_pixels[i * 4u + 2];
                    c[3] = ps2_alpha_to_u8(src_pixels[i * 4u + 3]);
                }
            } else if (image_type == 2) {
                if (i * 3u + 3u <= image_size) {
                    c[0] = src_pixels[i * 3u + 0];
                    c[1] = src_pixels[i * 3u + 1];
                    c[2] = src_pixels[i * 3u + 2];
                    c[3] = 255u;
                }
            } else if (image_type == 1) {
                if (i * 2u + 2u <= image_size) {
                    uint16_t raw = dc_le16(src_pixels + i * 2u);
                    c[0] = (uint8_t)(((raw & 31u) * 255u) / 31u);
                    c[1] = (uint8_t)((((raw >> 5) & 31u) * 255u) / 31u);
                    c[2] = (uint8_t)((((raw >> 10) & 31u) * 255u) / 31u);
                    c[3] = (raw & 0x8000u) ? 255u : 0u;
                }
            }

            size_t dst = ((size_t)y * (size_t)quad_w + (size_t)x) * 4u;
            rgba[dst + 0] = c[0];
            rgba[dst + 1] = c[1];
            rgba[dst + 2] = c[2];
            rgba[dst + 3] = c[3];
            if (c[3] > 4u) {
                if (x < min_x) min_x = x;
                if (x > max_x) max_x = x;
                if (y < min_y) min_y = y;
                if (y > max_y) max_y = y;
            }
        }
    }

    free(unswizzled);
    if (max_x < min_x || max_y < min_y) {
        free(rgba);
        return NULL;
    }

    int crop_w = max_x - min_x + 1;
    int crop_h = max_y - min_y + 1;
    SDL_Surface *surf = SDL_CreateSurface(crop_w, crop_h, SDL_PIXELFORMAT_RGBA32);
    if (!surf) {
        free(rgba);
        return NULL;
    }
    for (int y = 0; y < crop_h; y++) {
        uint8_t *dst_row = (uint8_t *)surf->pixels + (size_t)y * (size_t)surf->pitch;
        const uint8_t *src_row = rgba + ((size_t)(min_y + y) * (size_t)quad_w + (size_t)min_x) * 4u;
        memcpy(dst_row, src_row, (size_t)crop_w * 4u);
    }
    free(rgba);
    return surf;
}

static int selected_boot_language(void) {
    for (int i = 0; i < OPTION_COUNT; i++) {
        if (!strcmp(option_defs[i].setting, "boot.language")) {
            int c = SDL_clamp(option_values[i], 0, option_defs[i].count - 1);
            int lang = atoi(option_defs[i].values[c]);
            return (lang >= 1 && lang <= 5) ? lang : 0;
        }
    }
    return 0;
}

static void refresh_disc_logo(void) {
    ui.menu.logo = NULL;
    if (disc_logo) {
        SDL_DestroyTexture(disc_logo);
        disc_logo = NULL;
    }
    if (!renderer || disc_state != DISC_GOOD || !disc_path[0]) return;
    if (!port_cdvd_init(disc_path)) return;

    char lang_candidate[32] = "";
    int lang = selected_boot_language();
    if (lang >= 1 && lang <= 5) {
        snprintf(lang_candidate, sizeof lang_candidate, "title/title%d.img", lang);
    }
    const char *candidates[4];
    int cand_count = 0;
    if (lang_candidate[0]) candidates[cand_count++] = lang_candidate;
    candidates[cand_count++] = "title/title1.img";
    candidates[cand_count++] = "title/title0.img";

    for (int i = 0; i < cand_count && !disc_logo; i++) {
        size_t img_size = 0;
        uint8_t *img = read_disc_asset(candidates[i], &img_size);
        if (!img) continue;
        SDL_Surface *surf = decode_chro_from_img(img, img_size);
        free(img);
        if (surf) {
            disc_logo = SDL_CreateTextureFromSurface(renderer, surf);
            SDL_DestroySurface(surf);
            if (disc_logo) {
                SDL_SetTextureBlendMode(disc_logo, SDL_BLENDMODE_BLEND);
                SDL_SetTextureScaleMode(disc_logo, SDL_SCALEMODE_LINEAR);
            }
        }
    }

    port_cdvd_close();
    ui.menu.logo = disc_logo;
}

static void check_disc(const char *path) {
    struct sceCdlFILE boot;
    snprintf(disc_path, sizeof disc_path, "%s", path ? path : "");
    file_name(disc_path, disc_name, sizeof disc_name);
    disc_state = DISC_NONE;
    if (!*disc_path) {
        refresh_disc_logo();
        ph_launcher_rom_clear(&ui);
        return;
    }
    disc_state = port_cdvd_init(disc_path) && port_cdvd_find(PORT_EXECUTABLE_PATH, &boot) ? DISC_GOOD : DISC_WRONG;
    port_cdvd_close();
    refresh_disc_logo();
    if (disc_state == DISC_GOOD) {
        ph_launcher_rom_found(&ui, disc_name[0] ? disc_name : disc_path);
    } else {
        ph_launcher_rom_failed(&ui, disc_name[0] ? disc_name : disc_path, "Not a supported Dark Chronicle / Dark Cloud 2 disc");
    }
}

static int mod_clash(int index, char *why, size_t size) {
    char reason[LINE_SIZE];
    for (int other = 0; other < mod_count; other++) {
        if (mod_on[other] && other != index && port_pak_clash(index, other, reason, sizeof reason)) {
            snprintf(why, size, "Conflicts with %s (%s)", port_pak_text(other, "name"), reason);
            return 1;
        }
    }
    return 0;
}

static void mod_detail(int index, char *out, size_t size) {
    const char *description = port_pak_text(index, "description");
    const char *authors = port_pak_text(index, "authors");
    if (*description) snprintf(out, size, "%s", description);
    else if (*authors) snprintf(out, size, "By %s", authors);
    else snprintf(out, size, port_pak_files(index) == 1 ? "%d game file" : "%d game files", port_pak_files(index));
}

static void read_mods(void) {
    mod_count = SDL_min(port_paks_list(), MOD_LIMIT);
    char why[LINE_SIZE];
    for (int i = 0; i < mod_count; i++) {
        mod_on[i] = port_pak_enabled(i) && !mod_clash(i, why, sizeof why);
    }
}

static int ends_with(const char *text, const char *end) {
    size_t length = strlen(text), wanted = strlen(end);
    return length >= wanted && !SDL_strcasecmp(text + length - wanted, end);
}

static int copy_stream_file(const char *source, const char *target) {
    SDL_IOStream *in = SDL_IOFromFile(source, "rb");
    if (!in) return 0;
    SDL_IOStream *out = SDL_IOFromFile(target, "wb");
    if (!out) {
        SDL_CloseIO(in);
        return 0;
    }
    unsigned char chunk[65536];
    int ok = 1;
    for (;;) {
        size_t got = SDL_ReadIO(in, chunk, sizeof chunk);
        if (!got) {
            ok = SDL_GetIOStatus(in) == SDL_IO_STATUS_EOF;
            break;
        }
        if (SDL_WriteIO(out, chunk, got) != got) {
            ok = 0;
            break;
        }
    }
    SDL_CloseIO(in);
    if (!SDL_CloseIO(out)) ok = 0;
    if (!ok) SDL_RemovePath(target);
    return ok;
}

static void handle_picked_or_dropped_file(const char *path) {
    if (!path || !*path) return;
    char name[PATH_SIZE], target[PATH_SIZE * 2];
    file_name(path, name, sizeof name);
    if (!ends_with(name, ".dc2pak")) {
        check_disc(path);
        if (disc_state == DISC_GOOD) {
            if (ui.menu.page == PH_PAGE_ROM) ph_menu_open(&ui.menu, PH_PAGE_MAIN);
            ph_launcher_notice(&ui, "Game disc ready.");
        }
        return;
    }
    SDL_CreateDirectory(port_paks_folder());
    snprintf(target, sizeof target, "%s/%s", port_paks_folder(), name);
    int was[MOD_LIMIT];
    char ids[MOD_LIMIT][128];
    int count = mod_count;
    for (int i = 0; i < count; i++) {
        was[i] = mod_on[i];
        snprintf(ids[i], sizeof ids[i], "%s", port_pak_text(i, "id"));
    }
    if (!copy_stream_file(path, target)) {
        fprintf(stderr, "launcher: cannot copy %s: %s\n", path, SDL_GetError());
        ph_launcher_notice(&ui, "Could not import mod file.");
        return;
    }
    mod_count = SDL_min(port_paks_list(), MOD_LIMIT);
    for (int i = 0; i < mod_count; i++) {
        mod_on[i] = 0;
        for (int old = 0; old < count; old++) {
            if (was[old] && !strcmp(ids[old], port_pak_text(i, "id"))) mod_on[i] = 1;
        }
    }
    if (ui.menu.page != PAGE_MODS) ph_menu_push(&ui.menu, PAGE_MODS);
    ph_launcher_notice(&ui, "Mod imported.");
}

#if TARGET_OS_IPHONE
static void choose_saves(void) {
    port_ios_pick_saves(window);
    refresh_saves_name();
}

static void chosen_saves(const char *path) { (void)path; }

static void choose_disc(void) {
    char path[PATH_SIZE] = "";
    port_ios_pick_disc(window, path, sizeof path);
    if (path[0]) handle_picked_or_dropped_file(path);
}
#else
static void SDLCALL picked_file(void *data, const char *const *files, int filter) {
    (void)data;
    (void)filter;
    if (files && files[0]) snprintf(picked_path, sizeof picked_path, "%s", files[0]);
    else picked_path[0] = 0;
    SDL_SetAtomicInt(&picked, 1);
}

#ifdef __ANDROID__
static void android_picked(const char *path) {
    snprintf(picked_path, sizeof picked_path, "%s", path ? path : "");
    SDL_SetAtomicInt(&picked, 1);
}

static void choose_saves(void) {
    if (choosing || (window && (SDL_GetWindowFlags(window) & SDL_WINDOW_HIDDEN))) return;
    choosing = PICK_SAVES;
    port_android_pick_saves(saves_path[0] != 0, android_picked);
}

static void chosen_saves(const char *path) {
    if (!path || !path[0]) return;
    if (!strcmp(path, "-")) saves_path[0] = 0;
    else snprintf(saves_path, sizeof saves_path, "%s", path);
    refresh_saves_name();
}
#else
static int same_path(const char *a, const char *b) {
    while (*a && *b) {
        char ca = *a == '\\' ? '/' : *a;
        char cb = *b == '\\' ? '/' : *b;
#ifdef _WIN32
        if (SDL_tolower((unsigned char)ca) != SDL_tolower((unsigned char)cb)) return 0;
#else
        if (ca != cb) return 0;
#endif
        a++;
        b++;
    }
    return *a == *b;
}

static int strip_tail(char *path, const char *leaf) {
    char *slash = strrchr(path, '/');
    char *backslash = strrchr(path, '\\');
    if (backslash > slash) slash = backslash;
    if (!slash || SDL_strcasecmp(slash + 1, leaf)) return 0;
    *slash = 0;
    return 1;
}

static int is_default_saves(const char *path) {
    char def[PATH_SIZE];
    snprintf(def, sizeof def, "%s", port_memory_card_default_root());
    trim_path(def);
    if (same_path(path, def)) return 1;
    if (strip_tail(def, "saves") && same_path(path, def)) return 1;
    if (strip_tail(def, "DC2") && same_path(path, def)) return 1;
    return 0;
}

static void choose_saves(void) {
    if (choosing || (window && (SDL_GetWindowFlags(window) & SDL_WINDOW_HIDDEN))) return;
    choosing = PICK_SAVES;
    const char *current = env_saves[0] ? env_saves : saves_path[0] ? saves_path : port_memory_card_default_root();
    SDL_CreateDirectory(current);
    SDL_ShowOpenFolderDialog(picked_file, NULL, window, current, false);
}

static void chosen_saves(const char *path) {
    if (!path || !path[0]) return;
    env_saves[0] = 0;
    SDL_unsetenv_unsafe("DC2_SAVE_ROOT");
    if (!strcmp(path, "-")) {
        saves_path[0] = 0;
    } else {
        char clean[PATH_SIZE];
        snprintf(clean, sizeof clean, "%s", path);
        trim_path(clean);
        if (is_default_saves(clean)) saves_path[0] = 0;
        else snprintf(saves_path, sizeof saves_path, "%s", clean);
    }
    refresh_saves_name();
}
#endif

static void choose_file(const SDL_DialogFileFilter *filter, const char *start) {
    if (choosing || (window && (SDL_GetWindowFlags(window) & SDL_WINDOW_HIDDEN))) return;
    choosing = PICK_FILE;
#ifdef __ANDROID__
    (void)filter;
    (void)start;
    SDL_ShowOpenFileDialog(picked_file, NULL, window, NULL, 0, NULL, false);
#else
    SDL_ShowOpenFileDialog(picked_file, NULL, window, filter, 1, start, false);
#endif
}

static void choose_disc(void) {
    static const SDL_DialogFileFilter filter = {"Disc images", "iso;bin;img"};
    choose_file(&filter, disc_path[0] ? disc_path : NULL);
}
#endif

#ifdef __ANDROID__
static void open_mods_folder(void) {
    static const SDL_DialogFileFilter filter = {"Mods", "dc2pak"};
    choose_file(&filter, NULL);
}
#else
static void open_mods_folder(void) {
    char url[PATH_SIZE + 16];
    SDL_CreateDirectory(port_paks_folder());
    snprintf(url, sizeof url, "%s%s", DESKTOP ? "file://" : "shareddocuments://", port_paks_folder());
    SDL_OpenURL(url);
}
#endif

static void pick_disc_callback(struct ph_launcher *launcher, void *userdata) {
    (void)launcher;
    (void)userdata;
    choose_disc();
}

static void build_custom_page(struct ph_launcher *launcher, int page, void *userdata) {
    (void)userdata;
    struct ph_menu *menu = &launcher->menu;
    if (page == PAGE_MODS) {
        ph_menu_begin(menu, "Mods", PH_STYLE_LIST);
        if (mod_count == 0) {
            struct ph_row *r1 = ph_menu_add(menu, PH_ROW_NOTE, 0, 0, MODS_HINT_TOP, NULL);
            struct ph_row *r2 = ph_menu_add(menu, PH_ROW_NOTE, 0, 0, MODS_HINT_BOTTOM, NULL);
            if (r1) r1->tone = PH_TONE_DIM;
            if (r2) r2->tone = PH_TONE_DIM;
        }
        for (int i = 0; i < mod_count; i++) {
            char title[LINE_SIZE], detail[LINE_SIZE];
            snprintf(title, sizeof title, "%s  %s", port_pak_text(i, "name"), port_pak_text(i, "version"));
            int blocked = !mod_on[i] && mod_clash(i, detail, sizeof detail);
            if (!blocked) mod_detail(i, detail, sizeof detail);
            const char *state = mod_on[i] ? "On" : (blocked ? "Conflict" : "Off");
            struct ph_row *row = ph_launcher_add_custom(launcher, PH_ROW_VALUE, ACTION_MOD_BASE + i, title, state);
            if (row && blocked) row->disabled = 1;
            if (detail[0]) {
                struct ph_row *note = ph_menu_add(menu, PH_ROW_NOTE, 0, 0, detail, NULL);
                if (note) note->tone = blocked ? PH_TONE_ERROR : PH_TONE_DIM;
            }
        }
        ph_menu_add(menu, PH_ROW_GAP, 0, 0, NULL, NULL);
        ph_launcher_add_custom(launcher, PH_ROW_ACTION, ACTION_MODS_FOLDER, MODS_ACTION, NULL);
        return;
    }
    if (page == PAGE_SAVES) {
        ph_menu_begin(menu, "Saves", PH_STYLE_BIG);
        char line[LINE_SIZE];
        snprintf(line, sizeof line, "Location: %s", saves_name[0] ? saves_name : "Default");
        struct ph_row *note = ph_menu_add(menu, PH_ROW_NOTE, 0, 0, line, NULL);
        if (note) note->tone = PH_TONE_STRONG;
        if (DESKTOP && saves_path[0]) {
            struct ph_row *sub = ph_menu_add(menu, PH_ROW_NOTE, 0, 0, saves_path, NULL);
            if (sub) sub->tone = PH_TONE_DIM;
        }
        ph_menu_add(menu, PH_ROW_GAP, 0, 0, NULL, NULL);
        ph_launcher_add_custom(launcher, PH_ROW_ACTION, ACTION_CHOOSE_SAVES, "Choose saves folder", NULL);
        if (saves_path[0] || env_saves[0]) {
            ph_launcher_add_custom(launcher, PH_ROW_ACTION, ACTION_RESET_SAVES, "Use default folder", NULL);
        }
    }
}

static struct ph_launch handle_custom_action(struct ph_launcher *launcher, struct ph_menu_result result, void *userdata) {
    (void)userdata;
    int arg = result.argument;
    if (arg == ACTION_OPEN_MODS) {
        ph_menu_push(&launcher->menu, PAGE_MODS);
        return (struct ph_launch){ PH_LAUNCH_NONE, 0 };
    }
    if (arg == ACTION_OPEN_SAVES) {
        refresh_saves_name();
        ph_menu_push(&launcher->menu, PAGE_SAVES);
        return (struct ph_launch){ PH_LAUNCH_NONE, 0 };
    }
    if (arg == ACTION_MODS_FOLDER) {
        open_mods_folder();
        return (struct ph_launch){ PH_LAUNCH_NONE, 0 };
    }
    if (arg == ACTION_CHOOSE_SAVES) {
        choose_saves();
        return (struct ph_launch){ PH_LAUNCH_NONE, 0 };
    }
    if (arg == ACTION_RESET_SAVES) {
        chosen_saves("-");
        ph_launcher_notice(launcher, "Using default saves folder.");
        return (struct ph_launch){ PH_LAUNCH_SAVE_CONFIG, 0 };
    }
    if (arg >= ACTION_MOD_BASE && arg < ACTION_MOD_BASE + MOD_LIMIT) {
        int idx = arg - ACTION_MOD_BASE;
        if (idx >= 0 && idx < mod_count) {
            char why[LINE_SIZE];
            if (mod_on[idx]) mod_on[idx] = 0;
            else if (!mod_clash(idx, why, sizeof why)) mod_on[idx] = 1;
            else ph_launcher_notice(launcher, why);
            return (struct ph_launch){ PH_LAUNCH_SAVE_CONFIG, 0 };
        }
    }
    return (struct ph_launch){ PH_LAUNCH_NONE, 0 };
}

static float window_zoom(void) {
    SDL_Rect usable;
    if (!SDL_GetDisplayUsableBounds(SDL_GetPrimaryDisplay(), &usable)) return 1.0f;
    float zoom = SDL_min(usable.w * 0.9f / CANVAS_WIDTH, usable.h * 0.9f / CANVAS_HEIGHT);
    return SDL_clamp(zoom, 0.6f, (float)LARGEST_HEIGHT / CANVAS_HEIGHT);
}

static void make_textures(void) {
    if (backdrop) {
        SDL_DestroyTexture(backdrop);
        backdrop = NULL;
    }
    backdrop = ph_texture_png(renderer, backdrop_png, sizeof backdrop_png);
    ui.menu.background = backdrop;
    if (disc_state == DISC_GOOD) {
        refresh_disc_logo();
    }
}

static int open_window(int hidden) {
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
    float zoom = DESKTOP ? window_zoom() : 0.0f;
    SDL_WindowFlags flags = WINDOW_FLAGS | (hidden ? SDL_WINDOW_HIDDEN : 0);
    if (!SDL_CreateWindowAndRenderer("Dark Chronicle",
                                     DESKTOP ? (int)(CANVAS_WIDTH * zoom) : 0,
                                     DESKTOP ? (int)(CANVAS_HEIGHT * zoom) : 0,
                                     flags, &window, &renderer)) {
        fprintf(stderr, "launcher: %s\n", SDL_GetError());
        return 0;
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderVSync(renderer, 1);
    port_window_icon(window);
    return 1;
}

static void close_window(void) {
    ui.menu.background = NULL;
    ui.menu.logo = NULL;
    if (disc_logo) {
        SDL_DestroyTexture(disc_logo);
        disc_logo = NULL;
    }
    if (backdrop) {
        SDL_DestroyTexture(backdrop);
        backdrop = NULL;
    }
    if (renderer) {
        SDL_DestroyRenderer(renderer);
        renderer = NULL;
    }
    if (window) {
        SDL_DestroyWindow(window);
        window = NULL;
    }
}

static void feed_synth_key(SDL_Scancode scancode, SDL_Keycode keycode) {
    SDL_Event ev;
    SDL_zero(ev);
    ev.type = SDL_EVENT_KEY_DOWN;
    ev.key.scancode = scancode;
    ev.key.key = keycode;
    ev.key.down = true;
    struct ph_launch res = ph_launcher_event(&ui, renderer, &ev);
    refresh_option_visibility();
    (void)res;
}

static void take_picture(const char *path, const char *keys) {
    int pw = 0, ph = 0;
    SDL_GetRenderOutputSize(renderer, &pw, &ph);
    if (pw <= 0 || ph <= 0) { pw = CANVAS_WIDTH * 2; ph = CANVAS_HEIGHT * 2; }
    SDL_Texture *target = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, pw, ph);
    if (!target) return;
    SDL_SetRenderTarget(renderer, target);
    for (const char *k = keys; k && *k; k++) {
        if (*k == 'u') feed_synth_key(SDL_SCANCODE_UP, SDLK_UP);
        else if (*k == 'd') feed_synth_key(SDL_SCANCODE_DOWN, SDLK_DOWN);
        else if (*k == 'l') feed_synth_key(SDL_SCANCODE_LEFT, SDLK_LEFT);
        else if (*k == 'r') feed_synth_key(SDL_SCANCODE_RIGHT, SDLK_RIGHT);
        else if (*k == 'a') feed_synth_key(SDL_SCANCODE_RETURN, SDLK_RETURN);
        else if (*k == 'b') feed_synth_key(SDL_SCANCODE_ESCAPE, SDLK_ESCAPE);
    }
    SDL_SetRenderDrawColor(renderer, 10, 12, 20, 255);
    SDL_RenderClear(renderer);
    ph_launcher_draw(&ui, renderer);
    SDL_Surface *surface = SDL_RenderReadPixels(renderer, NULL);
    if (!surface) {
        fprintf(stderr, "launcher: no picture: %s\n", SDL_GetError());
    } else if (ends_with(path, ".png")) {
        if (!SDL_SavePNG(surface, path)) fprintf(stderr, "launcher: no png: %s\n", SDL_GetError());
    } else {
        if (!SDL_SaveBMP(surface, path)) fprintf(stderr, "launcher: no bmp: %s\n", SDL_GetError());
    }
    if (surface) SDL_DestroySurface(surface);
    SDL_SetRenderTarget(renderer, NULL);
    SDL_DestroyTexture(target);
}

static void ensure_settings_path(void) {
    if (settings_path[0]) return;
    const char *chosen = SDL_getenv("DC2TK_LAUNCHER_SETTINGS");
    char *folder = chosen ? NULL : SDL_GetPrefPath("DC2 Toolkit", "DC2");
    if (chosen) snprintf(settings_path, sizeof settings_path, "%s", chosen);
    else snprintf(settings_path, sizeof settings_path, "%slauncher.cfg", folder ? folder : "");
    SDL_free(folder);
}

void port_launcher_load_settings(void) {
    ensure_settings_path();
    if (settings_path[0]) port_settings_file(settings_path);
}

void port_launcher_save_setting(const char *key, const char *value) {
    if (!key || !value) return;
    port_setting_set(key, value);
    ensure_settings_path();
    if (!settings_path[0]) return;
    size_t size = 0;
    char *existing = (char *)SDL_LoadFile(settings_path, &size);
    char temp_path[PATH_SIZE + 16];
    snprintf(temp_path, sizeof temp_path, "%s.tmp", settings_path);
    FILE *out = fopen(temp_path, "w");
    if (!out) {
        SDL_free(existing);
        return;
    }
    size_t key_len = strlen(key);
    int replaced = 0;
    if (existing) {
        for (const char *at = existing; *at;) {
            size_t len = strcspn(at, "\r\n");
            if (len > key_len && !strncmp(at, key, key_len) && at[key_len] == '=') {
                if (!replaced) {
                    fprintf(out, "%s=%s\n", key, value);
                    replaced = 1;
                }
            } else {
                fprintf(out, "%.*s\n", (int)len, at);
            }
            at += len;
            if (*at == '\r') at++;
            if (*at == '\n') at++;
        }
        SDL_free(existing);
    }
    if (!replaced) fprintf(out, "%s=%s\n", key, value);
    if (fclose(out) == 0) {
#ifdef _WIN32
        remove(settings_path);
#endif
        rename(temp_path, settings_path);
    } else {
        remove(temp_path);
    }
}

int port_launcher(void) {
    port_launcher_load_settings();
    if (!DESKTOP) SDL_CreateDirectory(port_paks_folder());
    read_options();
    read_mods();

    const char *picture = SDL_getenv("DC2TK_LAUNCHER_PICTURE");
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        fprintf(stderr, "launcher: %s\n", SDL_GetError());
        return 0;
    }
    if (!open_window(picture != NULL)) {
        SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD);
        return 0;
    }

    chronicle_theme = ph_theme_make(0x1B2848, 0x141E38, 0x0D1324, 0xF2E6CE, 0xE8B948);
    ph_launcher_init(&ui, window);
    ui.menu.theme = &chronicle_theme;
    ui.settings = ph_options;
    ui.setting_count = OPTION_COUNT;
    ui.sections = section_names;
    ui.section_count = SEC_COUNT;
    ui.items = custom_items;
    ui.item_count = SDL_arraysize(custom_items);
    ui.rom_label = "Game disc";
    ui.rom_description = "Disc images";
    ui.rom_patterns = "iso;bin;img";
    ui.rom_hint = "Select your Dark Chronicle / Dark Cloud 2 PS2 disc image (.iso, .bin, .img).";
    ui.build_page = build_custom_page;
    ui.custom_action = handle_custom_action;
    ui.pick_rom = pick_disc_callback;
    make_textures();

    char starting[PATH_SIZE];
    snprintf(starting, sizeof starting, "%s", disc_path);
    check_disc(starting);
    ph_launcher_open(&ui, PH_PAGE_MAIN);

    if (picture) {
        take_picture(picture, SDL_getenv("DC2TK_LAUNCHER_KEYS"));
        close_window();
        SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD);
        return 0;
    }

    int playing = 0;
    int running = 1;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
                running = 0;
                break;
            }
            if (event.type == SDL_EVENT_RENDER_DEVICE_RESET || event.type == SDL_EVENT_RENDER_TARGETS_RESET) {
                make_textures();
            }
            if (event.type == SDL_EVENT_GAMEPAD_ADDED) {
                SDL_OpenGamepad(event.gdevice.which);
            }
            if (!DESKTOP && event.type == SDL_EVENT_DID_ENTER_FOREGROUND && disc_state != DISC_GOOD && disc_path[0]) {
                check_disc(disc_path);
            }
            if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat &&
                (event.key.key == SDLK_F11 || (event.key.key == SDLK_RETURN && (event.key.mod & SDL_KMOD_ALT)))) {
                bool fs = (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0;
                SDL_SetWindowFullscreen(window, !fs);
                continue;
            }
            if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat && event.key.key == SDLK_P &&
                ui.menu.page == PH_PAGE_MAIN && disc_state == DISC_GOOD) {
                playing = 1;
                running = 0;
                break;
            }
            struct ph_launch res = ph_launcher_event(&ui, renderer, &event);
            refresh_option_visibility();
            if (res.kind == PH_LAUNCH_PLAY) {
                if (disc_state == DISC_GOOD) {
                    playing = 1;
                    running = 0;
                    break;
                }
                choose_disc();
            } else if (res.kind == PH_LAUNCH_QUIT) {
                running = 0;
                break;
            } else if (res.kind == PH_LAUNCH_ROM_RESCAN) {
                if (disc_state != DISC_GOOD && disc_path[0]) check_disc(disc_path);
            } else if (res.kind == PH_LAUNCH_SETTING || res.kind == PH_LAUNCH_SAVE_CONFIG) {
                if (res.kind == PH_LAUNCH_SETTING && res.value >= 0 && res.value < OPTION_COUNT &&
                    !strcmp(option_defs[res.value].setting, "boot.language")) {
                    refresh_disc_logo();
                }
                if (!DESKTOP) save_options();
            }
        }
        if (!running) break;

        struct ph_launch upd = ph_launcher_update(&ui);
        if (upd.kind == PH_LAUNCH_ROM_PICKED && ui.picked[0]) {
            handle_picked_or_dropped_file(ui.picked);
            if (!DESKTOP) save_options();
        }

        if (SDL_GetAtomicInt(&picked)) {
            int kind = choosing;
            SDL_SetAtomicInt(&picked, 0);
            choosing = PICK_NONE;
            if (kind == PICK_SAVES) {
                chosen_saves(picked_path);
                if (picked_path[0]) ph_launcher_notice(&ui, "Saves folder updated.");
            } else if (picked_path[0]) {
                handle_picked_or_dropped_file(picked_path);
            }
            if (!DESKTOP) save_options();
        }

        SDL_SetRenderDrawColor(renderer, 10, 12, 20, 255);
        SDL_RenderClear(renderer);
        ph_launcher_draw(&ui, renderer);
        SDL_RenderPresent(renderer);
    }

    close_window();
    SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD);
    save_options();
    return playing;
}
