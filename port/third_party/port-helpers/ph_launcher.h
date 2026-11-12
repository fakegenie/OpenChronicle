#ifndef PH_LAUNCHER_H
#define PH_LAUNCHER_H

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

enum {
    PH_PAGE_MAIN,
    PH_PAGE_PAUSE,
    PH_PAGE_SETTINGS,
    PH_PAGE_MAPPINGS,
    PH_PAGE_ROM,
    PH_PAGE_LAN,
    PH_PAGE_WAITING,
    PH_PAGE_SAVE,
    PH_PAGE_LOAD,
    PH_PAGE_USER = 16,
    PH_PAGE_SECTION_BASE = 32,
    PH_PAGE_CONTROLS_SEC = 48,
    PH_PAGE_SYSTEM_SEC = 49,
};

static inline int ph_launcher_is_settings_page(int page)
{
    return page == PH_PAGE_SETTINGS || (page >= PH_PAGE_SECTION_BASE && page <= PH_PAGE_SYSTEM_SEC);
}

enum {
    PH_LAUNCH_NONE,
    PH_LAUNCH_PAUSE,
    PH_LAUNCH_PLAY,
    PH_LAUNCH_RESUME,
    PH_LAUNCH_RESET,
    PH_LAUNCH_END,
    PH_LAUNCH_QUIT,
    PH_LAUNCH_SAVE_STATE,
    PH_LAUNCH_LOAD_STATE,
    PH_LAUNCH_NETPLAY,
    PH_LAUNCH_SETTING,
    PH_LAUNCH_ROM_PICKED,
    PH_LAUNCH_ROM_RESCAN,
    PH_LAUNCH_CUSTOM,
    PH_LAUNCH_SAVE_CONFIG,
};

enum {
    PH_LAUNCHER_STATUS = 160,
    PH_LAUNCHER_SCRATCH = 12,
    PH_SETTINGS_ALL = -1,
};

struct ph_launch {
    int kind;
    int value;
};

struct ph_launcher_item {
    int page;
    int id;
    const char *label;
};

struct ph_launcher {
    struct ph_menu menu;
    SDL_Window *window;
    const struct ph_setting *settings;
    int setting_count;
    const char *const *sections;
    int section_count;
    struct ph_input *input;
    struct ph_touch *touch;
    struct ph_net *net;
    const struct ph_launcher_item *items;
    int item_count;
    int slots;
    const char *rom_hint;
    const char *rom_description;
    const char *rom_patterns;
    const char *rom_import;
    const char *rom_label;
    int rom_ready;
    int rom_removable;
    int hide_rom;
    int no_reset;
    int quit_from_pause;
    char rom_status[PH_LAUNCHER_STATUS];
    int open;
    int player;
    int arranging;
    int edit_argument;
    char picked[PH_PATH_SIZE];
    struct ph_setting scratch[PH_LAUNCHER_SCRATCH];
    int scratch_count;
    void (*build_page)(struct ph_launcher *launcher, int page, void *userdata);
    struct ph_launch (*custom_action)(struct ph_launcher *launcher, struct ph_menu_result result, void *userdata);
    void (*pick_rom)(struct ph_launcher *launcher, void *userdata);
    void *userdata;
};

void ph_launcher_init(struct ph_launcher *launcher, SDL_Window *window);
void ph_launcher_open(struct ph_launcher *launcher, int page);
void ph_launcher_close(struct ph_launcher *launcher);
struct ph_launch ph_launcher_event(struct ph_launcher *launcher, SDL_Renderer *renderer, const SDL_Event *event);
struct ph_launch ph_launcher_update(struct ph_launcher *launcher);
void ph_launcher_draw(struct ph_launcher *launcher, SDL_Renderer *renderer);
void ph_launcher_notice(struct ph_launcher *launcher, const char *text);
void ph_launcher_rom_clear(struct ph_launcher *launcher);
void ph_launcher_rom_found(struct ph_launcher *launcher, const char *path);
void ph_launcher_rom_failed(struct ph_launcher *launcher, const char *path, const char *problem);
void ph_launcher_rom_imported(struct ph_launcher *launcher);
struct ph_row *ph_launcher_add_custom(struct ph_launcher *launcher, int kind, int argument, const char *label, const char *value);

#ifdef __cplusplus
}
#endif

#endif

#if defined(PH_IMPLEMENTATION) && !defined(PH_LAUNCHER_IMPLEMENTED)
#define PH_LAUNCHER_IMPLEMENTED

#include "ph_view.h"

enum {
    PH_DO_PLAY = 1,
    PH_DO_RESUME,
    PH_DO_RESET,
    PH_DO_END,
    PH_DO_QUIT,
    PH_DO_PAGE,
    PH_DO_SLOT,
    PH_DO_SAVE_ONE,
    PH_DO_LOAD_ONE,
    PH_DO_SETTING,
    PH_DO_SCRATCH,
    PH_DO_DEFAULTS,
    PH_DO_PLAYER,
    PH_DO_BINDING,
    PH_DO_RESET_BINDINGS,
    PH_DO_ARRANGE,
    PH_DO_RESET_LAYOUT,
    PH_DO_PICK,
    PH_DO_REMOVE,
    PH_DO_HOST,
    PH_DO_GAME,
    PH_DO_CANCEL,
    PH_DO_CUSTOM,
};

static struct ph_launch ph_launch(int kind, int value)
{
    return (struct ph_launch){ kind, value };
}

void ph_launcher_init(struct ph_launcher *launcher, SDL_Window *window)
{
    SDL_zerop(launcher);
    ph_menu_init(&launcher->menu, NULL, PH_PAGE_MAIN);
    launcher->window = window;
    launcher->rom_description = "ROM files";
    launcher->rom_patterns = "zip";
    launcher->open = 1;
}

static int ph_launcher_netplay(const struct ph_launcher *launcher)
{
    return launcher->net && launcher->net->status == PH_NET_CONNECTED;
}

static int ph_launcher_in_game(const struct ph_launcher *launcher)
{
    return !launcher->open || launcher->menu.stack[0] == PH_PAGE_PAUSE;
}

static void ph_launcher_enter(struct ph_launcher *launcher, int page)
{
    if (page == PH_PAGE_LAN && launcher->net) {
        ph_net_search(launcher->net);
    }
}

void ph_launcher_open(struct ph_launcher *launcher, int page)
{
    if (launcher->touch && launcher->arranging) {
        ph_touch_edit(launcher->touch, 0);
    }
    launcher->arranging = 0;
    launcher->open = 1;
    ph_menu_open(&launcher->menu, page);
    ph_launcher_enter(launcher, page);
    SDL_FlushEvents(SDL_EVENT_MOUSE_MOTION, SDL_EVENT_MOUSE_WHEEL);
    if (launcher->touch) {
        ph_touch_release(launcher->touch);
    }
}

void ph_launcher_close(struct ph_launcher *launcher)
{
    launcher->open = 0;
    ph_menu_stop_editing(&launcher->menu);
    if (launcher->input) {
        launcher->input->capturing = 0;
        ph_input_block_held(launcher->input);
    }
    if (launcher->touch && launcher->arranging) {
        ph_touch_edit(launcher->touch, 0);
    }
    if (launcher->touch) {
        ph_touch_release(launcher->touch);
    }
    launcher->arranging = 0;
}

static int ph_launcher_pauses(const struct ph_launcher *launcher, const SDL_Event *event)
{
    if (event->type == SDL_EVENT_KEY_DOWN && !event->key.repeat) {
        return event->key.scancode == SDL_SCANCODE_ESCAPE || event->key.scancode == SDL_SCANCODE_AC_BACK;
    }
    if (event->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
        return event->gbutton.button == SDL_GAMEPAD_BUTTON_GUIDE || event->gbutton.button == SDL_GAMEPAD_BUTTON_BACK;
    }
    if (event->type == SDL_EVENT_WINDOW_FOCUS_LOST) {
        return !ph_launcher_netplay(launcher);
    }
    return 0;
}

static struct ph_launch ph_launcher_pause(struct ph_launcher *launcher)
{
    ph_launcher_open(launcher, PH_PAGE_PAUSE);
    return ph_launch(PH_LAUNCH_PAUSE, 0);
}

void ph_launcher_notice(struct ph_launcher *launcher, const char *text)
{
    ph_menu_notice(&launcher->menu, text);
}

void ph_launcher_rom_clear(struct ph_launcher *launcher)
{
    launcher->rom_ready = 0;
    launcher->rom_removable = 0;
    launcher->rom_status[0] = 0;
}

void ph_launcher_rom_found(struct ph_launcher *launcher, const char *path)
{
    launcher->rom_ready = 1;
    launcher->rom_removable = launcher->rom_import && ph_in_user_folder(path);
    SDL_strlcpy(launcher->rom_status, ph_file_name(path), sizeof launcher->rom_status);
}

void ph_launcher_rom_failed(struct ph_launcher *launcher, const char *path, const char *problem)
{
    launcher->rom_ready = 0;
    launcher->rom_removable = 0;
    SDL_snprintf(launcher->rom_status, sizeof launcher->rom_status, "%s %s", ph_file_name(path), problem);
}

void ph_launcher_rom_imported(struct ph_launcher *launcher)
{
    if (launcher->rom_ready) {
        ph_launcher_open(launcher, PH_PAGE_MAIN);
    }
    ph_launcher_notice(launcher, launcher->rom_ready ? "ROM loaded." : launcher->rom_status);
}

static void ph_launcher_push(struct ph_launcher *launcher, int page)
{
    ph_menu_push(&launcher->menu, page);
    ph_launcher_enter(launcher, page);
}

struct ph_row *ph_launcher_add_custom(struct ph_launcher *launcher, int kind, int argument, const char *label, const char *value)
{
    return ph_menu_add(&launcher->menu, kind, PH_DO_CUSTOM, argument, label, value);
}

static const char *ph_launcher_rom_title(const struct ph_launcher *launcher)
{
    return launcher->rom_label && launcher->rom_label[0] ? launcher->rom_label : "ROM";
}

static void ph_launcher_add_items(struct ph_launcher *launcher, int page)
{
    for (int index = 0; index < launcher->item_count; index++) {
        if (launcher->items[index].page == page) {
            ph_menu_add(&launcher->menu, PH_ROW_ACTION, PH_DO_CUSTOM, launcher->items[index].id, launcher->items[index].label, NULL);
        }
    }
}

static void ph_launcher_note(struct ph_launcher *launcher, const char *text, int tone)
{
    struct ph_row *row = ph_menu_add(&launcher->menu, PH_ROW_NOTE, 0, 0, text, NULL);
    if (row) {
        row->tone = tone;
    }
}

static void ph_launcher_add_setting(struct ph_launcher *launcher, int id, int index, const struct ph_setting *setting)
{
    char text[PH_MENU_TEXT];
    if (!ph_setting_shown(setting)) {
        return;
    }
    ph_setting_text(setting, text, sizeof text);
    int kind = setting->text ? PH_ROW_ACTION : ph_setting_adjustable(setting) ? PH_ROW_VALUE : PH_ROW_NOTE;
    struct ph_row *row = ph_menu_add(&launcher->menu, kind, id, index, setting->label, text);
    if (row && kind == PH_ROW_VALUE && !setting->choices && setting->value && setting->maximum > setting->minimum) {
        int clamped = SDL_clamp(*setting->value, setting->minimum, setting->maximum);
        row->has_slider = 1;
        row->slider = (float)(clamped - setting->minimum) / (float)(setting->maximum - setting->minimum);
    }
}

static void ph_launcher_build_main(struct ph_launcher *launcher)
{
    struct ph_menu *menu = &launcher->menu;
    ph_menu_add(menu, PH_ROW_ACTION, launcher->rom_ready ? PH_DO_PLAY : PH_DO_PAGE, PH_PAGE_ROM, "Play", NULL);
    if (launcher->net) {
        ph_menu_add(menu, PH_ROW_ACTION, PH_DO_PAGE, PH_PAGE_LAN, "LAN play", NULL)->disabled = !launcher->rom_ready;
    }
    ph_launcher_add_items(launcher, PH_PAGE_MAIN);
    ph_menu_add(menu, PH_ROW_ACTION, PH_DO_PAGE, PH_PAGE_SETTINGS, "Settings", NULL);
    if (!PH_MOBILE) {
        ph_menu_add(menu, PH_ROW_ACTION, PH_DO_QUIT, 0, "Quit", NULL);
    }
    if (!launcher->rom_ready) {
        char empty[PH_LAUNCHER_STATUS];
        SDL_snprintf(empty, sizeof empty, "No %s yet. Choose Play to add one.", ph_launcher_rom_title(launcher));
        ph_launcher_note(launcher, launcher->rom_status[0] ? launcher->rom_status : empty, launcher->rom_status[0] ? PH_TONE_ERROR : PH_TONE_NORMAL);
    }
}

static void ph_launcher_build_pause(struct ph_launcher *launcher)
{
    struct ph_menu *menu = &launcher->menu;
    int netplay = ph_launcher_netplay(launcher);
    ph_menu_add(menu, PH_ROW_ACTION, PH_DO_RESUME, 0, "Resume", NULL);
    if (launcher->slots == 1 && !netplay) {
        ph_menu_add(menu, PH_ROW_ACTION, PH_DO_SAVE_ONE, 1, "Save state", NULL);
        ph_menu_add(menu, PH_ROW_ACTION, PH_DO_LOAD_ONE, 1, "Load state", NULL);
    } else if (launcher->slots > 1 && !netplay) {
        ph_menu_add(menu, PH_ROW_ACTION, PH_DO_PAGE, PH_PAGE_SAVE, "Save state", NULL);
        ph_menu_add(menu, PH_ROW_ACTION, PH_DO_PAGE, PH_PAGE_LOAD, "Load state", NULL);
    }
    if (!netplay && !launcher->no_reset) {
        ph_menu_add(menu, PH_ROW_ACTION, PH_DO_RESET, 0, "Reset", NULL);
    }
    ph_launcher_add_items(launcher, PH_PAGE_PAUSE);
    ph_menu_add(menu, PH_ROW_ACTION, PH_DO_PAGE, PH_PAGE_SETTINGS, "Settings", NULL);
    if (launcher->quit_from_pause && !netplay) {
        ph_menu_add(menu, PH_ROW_ACTION, PH_DO_QUIT, 0, "Quit", NULL);
    } else {
        ph_menu_add(menu, PH_ROW_ACTION, PH_DO_END, 0, netplay ? "Leave LAN game" : "Quit to launcher", NULL);
    }
}

static int ph_launcher_section_total(const struct ph_launcher *launcher)
{
    return SDL_clamp(SDL_max(launcher->section_count, 1), 1, PH_MENU_SECTIONS);
}

static const char *ph_launcher_section_title(const struct ph_launcher *launcher, int section)
{
    if (section >= 0 && section < launcher->section_count && launcher->sections && launcher->sections[section]) {
        return launcher->sections[section];
    }
    return "Display";
}

static int ph_launcher_named_section(const struct ph_launcher *launcher, const char *name)
{
    int sections = ph_launcher_section_total(launcher);
    for (int s = 0; s < sections; s++) {
        if (!SDL_strcasecmp(ph_launcher_section_title(launcher, s), name)) {
            return s;
        }
    }
    return -1;
}

static int ph_launcher_has_controls(const struct ph_launcher *launcher)
{
    return launcher->input != NULL || launcher->touch != NULL;
}

static int ph_launcher_has_system(const struct ph_launcher *launcher)
{
    if (!launcher->hide_rom || launcher->setting_count > 0 || launcher->touch != NULL) {
        return 1;
    }
    for (int i = 0; i < launcher->item_count; i++) {
        if (launcher->items[i].page == PH_PAGE_SETTINGS) {
            return 1;
        }
    }
    return 0;
}

static int ph_launcher_section_populated(const struct ph_launcher *launcher, int section)
{
    int sections = ph_launcher_section_total(launcher);
    for (int i = 0; i < launcher->setting_count; i++) {
        const struct ph_setting *setting = &launcher->settings[i];
        if (SDL_clamp(setting->section, 0, sections - 1) == section && ph_setting_shown(setting)) {
            return 1;
        }
    }
    if (ph_launcher_has_controls(launcher) && ph_launcher_named_section(launcher, "Controls") == section) {
        return 1;
    }
    if (ph_launcher_has_system(launcher) && ph_launcher_named_section(launcher, "System") == section) {
        return 1;
    }
    return 0;
}

static void ph_launcher_append_controls_rows(struct ph_launcher *launcher)
{
    struct ph_menu *menu = &launcher->menu;
    if (launcher->input) {
        ph_menu_add(menu, PH_ROW_ACTION, PH_DO_PAGE, PH_PAGE_MAPPINGS, "Mappings", NULL);
    }
    if (launcher->touch) {
        int base = launcher->scratch_count;
        int added = ph_touch_settings(launcher->touch, &launcher->scratch[base]);
        launcher->scratch_count += added;
        for (int index = 0; index < added; index++) {
            ph_launcher_add_setting(launcher, PH_DO_SCRATCH, base + index, &launcher->scratch[base + index]);
        }
        ph_menu_add(menu, PH_ROW_ACTION, PH_DO_ARRANGE, 0, "Arrange touch controls", NULL);
        if (ph_touch_customized(launcher->touch)) {
            ph_menu_add(menu, PH_ROW_ACTION, PH_DO_RESET_LAYOUT, 0, "Reset touch layout", NULL);
        }
    }
}

static void ph_launcher_append_system_rows(struct ph_launcher *launcher)
{
    struct ph_menu *menu = &launcher->menu;
    ph_launcher_add_items(launcher, PH_PAGE_SETTINGS);
    if (!launcher->hide_rom) {
        const char *rom = launcher->rom_ready ? (launcher->rom_status[0] ? launcher->rom_status : "Ready") : "Choose...";
        ph_menu_add(menu, PH_ROW_ACTION, PH_DO_PAGE, PH_PAGE_ROM, ph_launcher_rom_title(launcher), rom);
    }
    if (launcher->setting_count || launcher->touch) {
        ph_menu_add(menu, PH_ROW_ACTION, PH_DO_DEFAULTS, 0, "Restore defaults", NULL);
    }
}

static void ph_launcher_build_settings(struct ph_launcher *launcher)
{
    struct ph_menu *menu = &launcher->menu;
    int sections = ph_launcher_section_total(launcher);
    int controls_sec = ph_launcher_named_section(launcher, "Controls");
    int system_sec = ph_launcher_named_section(launcher, "System");

    for (int s = 0; s < sections; s++) {
        if (ph_launcher_section_populated(launcher, s)) {
            ph_menu_add(menu, PH_ROW_ACTION, PH_DO_PAGE, PH_PAGE_SECTION_BASE + s, ph_launcher_section_title(launcher, s), NULL);
        }
    }
    if (ph_launcher_has_controls(launcher) && controls_sec < 0) {
        ph_menu_add(menu, PH_ROW_ACTION, PH_DO_PAGE, PH_PAGE_CONTROLS_SEC, "Controls", NULL);
    }
    if (ph_launcher_has_system(launcher) && system_sec < 0) {
        ph_menu_add(menu, PH_ROW_ACTION, PH_DO_PAGE, PH_PAGE_SYSTEM_SEC, "System", NULL);
    }
}

static void ph_launcher_build_settings_section(struct ph_launcher *launcher, int section)
{
    int sections = ph_launcher_section_total(launcher);
    section = SDL_clamp(section, 0, sections - 1);
    for (int index = 0; index < launcher->setting_count; index++) {
        const struct ph_setting *setting = &launcher->settings[index];
        if (SDL_clamp(setting->section, 0, sections - 1) == section && ph_setting_shown(setting)) {
            ph_launcher_add_setting(launcher, PH_DO_SETTING, index, setting);
        }
    }
    if (ph_launcher_has_controls(launcher) && ph_launcher_named_section(launcher, "Controls") == section) {
        ph_launcher_append_controls_rows(launcher);
    }
    if (ph_launcher_has_system(launcher) && ph_launcher_named_section(launcher, "System") == section) {
        ph_launcher_append_system_rows(launcher);
    }
}

static void ph_launcher_binding_text(const struct ph_launcher *launcher, int action, int slot, char *text, int size)
{
    const struct ph_input *input = launcher->input;
    int waiting = input->capturing && input->capture_player == launcher->player && input->capture_action == action && input->capture_slot == slot;
    if (waiting) {
        SDL_strlcpy(text, "Press...", (size_t)size);
    } else {
        ph_input_binding_text(input, launcher->player, action, slot, text, size);
    }
}

static void ph_launcher_build_mappings(struct ph_launcher *launcher)
{
    struct ph_menu *menu = &launcher->menu;
    struct ph_input *input = launcher->input;
    char text[PH_MENU_TEXT];
    launcher->player = SDL_clamp(launcher->player, 0, input->players - 1);
    if (input->players > 1) {
        SDL_snprintf(text, sizeof text, "%d", launcher->player + 1);
        ph_menu_add(menu, PH_ROW_VALUE, PH_DO_PLAYER, 0, "Player", text);
    }
    const char *pad = ph_input_pad_name(input, launcher->player);
    if (pad) {
        SDL_snprintf(text, sizeof text, "Gamepad: %s", pad);
    } else {
        SDL_snprintf(text, sizeof text, "No gamepad connected for player %d", launcher->player + 1);
    }
    ph_launcher_note(launcher, text, PH_TONE_DIM);
    struct ph_row *columns = ph_menu_add(menu, PH_ROW_HEADING, 0, 0, "Button", "Keyboard");
    if (columns) {
        SDL_strlcpy(columns->other, "Gamepad", sizeof columns->other);
    }
    for (int action = 0; action < input->actions; action++) {
        ph_launcher_binding_text(launcher, action, PH_SLOT_KEY, text, sizeof text);
        struct ph_row *row = ph_menu_add(menu, PH_ROW_PAIR, PH_DO_BINDING, action, input->action_labels[action], text);
        if (row) {
            ph_launcher_binding_text(launcher, action, PH_SLOT_PAD, row->other, sizeof row->other);
        }
    }
    ph_menu_add(menu, PH_ROW_ACTION, PH_DO_RESET_BINDINGS, 0, "Reset to defaults", NULL);
    ph_launcher_add_items(launcher, PH_PAGE_MAPPINGS);
}

static void ph_launcher_build_rom(struct ph_launcher *launcher)
{
    struct ph_menu *menu = &launcher->menu;
    if (launcher->rom_hint) {
        ph_launcher_note(launcher, launcher->rom_hint, PH_TONE_NORMAL);
    }
    ph_launcher_note(launcher, PH_MOBILE ? "Pick it from Files to copy it into the app." : "Pick the file, or drop it on this window.", PH_TONE_DIM);
    ph_menu_add(menu, PH_ROW_GAP, 0, 0, NULL, NULL);
    if (launcher->rom_ready) {
        ph_launcher_note(launcher, launcher->rom_status[0] ? launcher->rom_status : "ROM ready", PH_TONE_STRONG);
    } else {
        ph_launcher_note(launcher, "No ROM loaded", PH_TONE_WARNING);
        if (launcher->rom_status[0]) {
            ph_launcher_note(launcher, launcher->rom_status, PH_TONE_ERROR);
        }
    }
    if (ph_launcher_in_game(launcher)) {
        ph_launcher_note(launcher, "Quit to the launcher to change the ROM.", PH_TONE_DIM);
        return;
    }
    ph_menu_add(menu, PH_ROW_ACTION, PH_DO_PICK, 0, PH_MOBILE ? "Choose from Files" : "Choose file", NULL)->disabled = ph_picking();
    ph_launcher_add_items(launcher, PH_PAGE_ROM);
    if (launcher->rom_removable) {
        ph_menu_add(menu, PH_ROW_ACTION, PH_DO_REMOVE, 0, "Remove imported ROM", NULL);
    }
}

static void ph_launcher_build_lan(struct ph_launcher *launcher)
{
    struct ph_menu *menu = &launcher->menu;
    struct ph_net *net = launcher->net;
    ph_menu_add(menu, PH_ROW_ACTION, PH_DO_HOST, 0, "Host", NULL);
    ph_menu_add(menu, PH_ROW_HEADING, 0, 0, "Games on this network", NULL);
    for (int index = 0; index < net->game_count; index++) {
        const struct ph_net_game *game = &net->games[index];
        ph_menu_add(menu, PH_ROW_ACTION, PH_DO_GAME, index, game->name, game->other_version ? "Other version" : game->address)->disabled = game->other_version;
    }
    if (!net->game_count) {
        ph_menu_add(menu, PH_ROW_NOTE, 0, 0, net->searching ? "Looking for games..." : "Not searching", NULL);
    }
}

static void ph_launcher_build_waiting(struct ph_launcher *launcher)
{
    struct ph_net *net = launcher->net;
    ph_launcher_note(launcher, net->message, PH_TONE_STRONG);
    if (net->status == PH_NET_HOSTING) {
        ph_launcher_note(launcher, "On the other device choose LAN play, then pick your name.", PH_TONE_DIM);
        launcher->scratch_count = ph_net_settings(net, launcher->scratch);
        ph_launcher_add_setting(launcher, PH_DO_SCRATCH, 0, &launcher->scratch[0]);
    }
    ph_menu_add(&launcher->menu, PH_ROW_ACTION, PH_DO_CANCEL, 0, "Cancel", NULL);
}

static void ph_launcher_build_slots(struct ph_launcher *launcher, int saving)
{
    for (int slot = 1; slot <= SDL_min(launcher->slots, PH_MENU_ROWS - 2); slot++) {
        char path[PH_PATH_SIZE];
        char label[PH_MENU_TEXT];
        char when[PH_MENU_TEXT];
        int64_t time = ph_slot_path(slot, path, sizeof path) ? ph_file_time(path) : 0;
        ph_time_text(time, when, sizeof when);
        SDL_snprintf(label, sizeof label, "Slot %d", slot);
        ph_menu_add(&launcher->menu, PH_ROW_ACTION, PH_DO_SLOT, slot, label, when)->disabled = !saving && time <= 0;
    }
}

static const char *ph_launcher_page_title(const struct ph_launcher *launcher, int page)
{
    if (page >= PH_PAGE_SECTION_BASE && page < PH_PAGE_SECTION_BASE + PH_MENU_SECTIONS) {
        return ph_launcher_section_title(launcher, page - PH_PAGE_SECTION_BASE);
    }
    switch (page) {
    case PH_PAGE_SETTINGS:
        return "Settings";
    case PH_PAGE_CONTROLS_SEC:
        return "Controls";
    case PH_PAGE_SYSTEM_SEC:
        return "System";
    case PH_PAGE_MAPPINGS:
        return "Mappings";
    case PH_PAGE_ROM:
        return ph_launcher_rom_title(launcher);
    case PH_PAGE_LAN:
        return "LAN play";
    case PH_PAGE_WAITING:
        return launcher->net && launcher->net->status == PH_NET_HOSTING ? "Hosting" : "Joining";
    case PH_PAGE_SAVE:
        return "Save state";
    case PH_PAGE_LOAD:
        return "Load state";
    default:
        return NULL;
    }
}

static int ph_launcher_style(int page)
{
    switch (page) {
    case PH_PAGE_MAIN:
    case PH_PAGE_PAUSE:
    case PH_PAGE_SETTINGS:
    case PH_PAGE_ROM:
        return PH_STYLE_BIG;
    default:
        return PH_STYLE_LIST;
    }
}

static void ph_launcher_populate_page(struct ph_launcher *launcher, int page)
{
    ph_menu_begin(&launcher->menu, ph_launcher_page_title(launcher, page), ph_launcher_style(page));
    launcher->scratch_count = 0;
    if (page >= PH_PAGE_SECTION_BASE && page < PH_PAGE_SECTION_BASE + PH_MENU_SECTIONS) {
        ph_launcher_build_settings_section(launcher, page - PH_PAGE_SECTION_BASE);
        return;
    }
    switch (page) {
    case PH_PAGE_PAUSE:
        ph_launcher_build_pause(launcher);
        break;
    case PH_PAGE_SETTINGS:
        ph_launcher_build_settings(launcher);
        break;
    case PH_PAGE_CONTROLS_SEC:
        ph_launcher_append_controls_rows(launcher);
        break;
    case PH_PAGE_SYSTEM_SEC:
        ph_launcher_append_system_rows(launcher);
        break;
    case PH_PAGE_MAPPINGS:
        ph_launcher_build_mappings(launcher);
        break;
    case PH_PAGE_ROM:
        ph_launcher_build_rom(launcher);
        break;
    case PH_PAGE_LAN:
        ph_launcher_build_lan(launcher);
        break;
    case PH_PAGE_WAITING:
        ph_launcher_build_waiting(launcher);
        break;
    case PH_PAGE_SAVE:
    case PH_PAGE_LOAD:
        ph_launcher_build_slots(launcher, page == PH_PAGE_SAVE);
        break;
    default:
        if (page >= PH_PAGE_USER && page < PH_PAGE_SECTION_BASE && launcher->build_page) {
            launcher->build_page(launcher, page, launcher->userdata);
        } else {
            ph_launcher_build_main(launcher);
        }
        break;
    }
}

static void ph_launcher_sync_parent_selection(struct ph_menu_layer *layer, int child_page)
{
    if (!layer->valid || layer->count <= 0) {
        return;
    }
    if (layer->selected >= 0 && layer->selected < layer->count) {
        const struct ph_row *cur = &layer->rows[layer->selected];
        if (cur->id == PH_DO_PAGE && cur->argument == child_page) {
            return;
        }
        if (child_page == PH_PAGE_WAITING && (cur->id == PH_DO_HOST || cur->id == PH_DO_GAME)) {
            return;
        }
    }
    for (int i = 0; i < layer->count; i++) {
        const struct ph_row *row = &layer->rows[i];
        if (row->id == PH_DO_PAGE && row->argument == child_page) {
            layer->selected = i;
            return;
        }
        if (child_page == PH_PAGE_WAITING && row->id == PH_DO_HOST) {
            layer->selected = i;
            return;
        }
    }
}

static void ph_launcher_build(struct ph_launcher *launcher)
{
    struct ph_menu *menu = &launcher->menu;
    int page = menu->page;
    int lan = page == PH_PAGE_LAN || page == PH_PAGE_WAITING;
    if ((lan && !launcher->net) || (page == PH_PAGE_MAPPINGS && !launcher->input)) {
        ph_menu_open(menu, PH_PAGE_MAIN);
        page = PH_PAGE_MAIN;
    }
    menu->over_game = menu->stack[0] == PH_PAGE_PAUSE;

    int active = SDL_clamp(menu->depth - 1, 0, PH_MENU_DEPTH - 1);
    for (int lvl = 0; lvl < active; lvl++) {
        int parent_page = SDL_clamp(menu->stack[lvl], 0, PH_MENU_PAGES - 1);
        menu->page = parent_page;
        ph_launcher_populate_page(launcher, parent_page);
        if (menu->count > 0) {
            menu->selected[parent_page] = SDL_clamp(menu->selected[parent_page], 0, menu->count - 1);
        }
        ph_menu_snapshot_layer(menu, lvl);
        ph_launcher_sync_parent_selection(&menu->layers[lvl], menu->stack[lvl + 1]);
    }

    menu->page = page;
    ph_launcher_populate_page(launcher, page);
    ph_menu_end(menu);
}

static const struct ph_setting *ph_launcher_setting(const struct ph_launcher *launcher, int id, int argument)
{
    if (id == PH_DO_SETTING && argument >= 0 && argument < launcher->setting_count) {
        return &launcher->settings[argument];
    }
    if (id == PH_DO_SCRATCH && argument >= 0 && argument < launcher->scratch_count) {
        return &launcher->scratch[argument];
    }
    return NULL;
}

static struct ph_launch ph_launcher_changed(int id, int argument)
{
    return id == PH_DO_SETTING ? ph_launch(PH_LAUNCH_SETTING, argument) : ph_launch(PH_LAUNCH_SAVE_CONFIG, 0);
}

static struct ph_launch ph_launcher_adjust(struct ph_launcher *launcher, int id, int argument, int delta, int wrap)
{
    if (id == PH_DO_PLAYER && launcher->input) {
        int players = launcher->input->players;
        launcher->player = (launcher->player + delta + players) % players;
        return ph_launch(PH_LAUNCH_NONE, 0);
    }
    const struct ph_setting *setting = ph_launcher_setting(launcher, id, argument);
    if (!setting) {
        return ph_launch(PH_LAUNCH_NONE, 0);
    }
    if (setting->text) {
        launcher->edit_argument = argument;
        ph_menu_edit(&launcher->menu, launcher->window, id, setting->text, 0);
        return ph_launch(PH_LAUNCH_NONE, 0);
    }
    ph_setting_step(setting, delta, wrap);
    return ph_launcher_changed(id, argument);
}

static struct ph_launch ph_launcher_connect(struct ph_launcher *launcher, int started)
{
    if (started) {
        ph_launcher_push(launcher, PH_PAGE_WAITING);
    } else {
        ph_launcher_notice(launcher, launcher->net->message);
    }
    return ph_launch(PH_LAUNCH_SAVE_CONFIG, 0);
}

static struct ph_launch ph_launcher_finish_edit(struct ph_launcher *launcher, int id)
{
    const struct ph_setting *setting = ph_launcher_setting(launcher, id, launcher->edit_argument);
    if (!setting || !setting->text) {
        return ph_launch(PH_LAUNCH_NONE, 0);
    }
    SDL_strlcpy(setting->text, launcher->menu.edit_text, (size_t)setting->text_size);
    return ph_launcher_changed(id, launcher->edit_argument);
}

static struct ph_launch ph_launcher_back(struct ph_launcher *launcher)
{
    int page = launcher->menu.page;
    if (page == PH_PAGE_PAUSE) {
        return ph_launch(PH_LAUNCH_RESUME, 0);
    }
    if (page == PH_PAGE_WAITING && launcher->net) {
        ph_net_stop(launcher->net);
        ph_net_search(launcher->net);
    }
    if (page == PH_PAGE_LAN && launcher->net && !ph_launcher_netplay(launcher)) {
        ph_net_stop(launcher->net);
    }
    ph_menu_pop(&launcher->menu);
    return ph_launch(PH_LAUNCH_NONE, 0);
}

static struct ph_launch ph_launcher_activate(struct ph_launcher *launcher, struct ph_menu_result result)
{
    switch (result.id) {
    case PH_DO_PLAY:
        return ph_launch(PH_LAUNCH_PLAY, 0);
    case PH_DO_RESUME:
        return ph_launch(PH_LAUNCH_RESUME, 0);
    case PH_DO_RESET:
        return ph_launch(PH_LAUNCH_RESET, 0);
    case PH_DO_END:
        if (launcher->net) {
            ph_net_stop(launcher->net);
        }
        ph_launcher_open(launcher, PH_PAGE_MAIN);
        return ph_launch(PH_LAUNCH_END, 0);
    case PH_DO_QUIT:
        return ph_launch(PH_LAUNCH_QUIT, 0);
    case PH_DO_PAGE:
        ph_launcher_push(launcher, result.argument);
        return ph_launch(PH_LAUNCH_NONE, 0);
    case PH_DO_SLOT:
        return ph_launch(launcher->menu.page == PH_PAGE_SAVE ? PH_LAUNCH_SAVE_STATE : PH_LAUNCH_LOAD_STATE, result.argument);
    case PH_DO_SAVE_ONE:
        return ph_launch(PH_LAUNCH_SAVE_STATE, 1);
    case PH_DO_LOAD_ONE:
        return ph_launch(PH_LAUNCH_LOAD_STATE, 1);
    case PH_DO_SETTING:
    case PH_DO_SCRATCH:
    case PH_DO_PLAYER:
        return ph_launcher_adjust(launcher, result.id, result.argument, 1, 1);
    case PH_DO_DEFAULTS:
        ph_settings_reset(launcher->settings, launcher->setting_count);
        ph_settings_reset(launcher->scratch, launcher->scratch_count);
        if (launcher->touch) {
            struct ph_setting touch_defs[PH_LAUNCHER_SCRATCH];
            int touch_count = ph_touch_settings(launcher->touch, touch_defs);
            ph_settings_reset(touch_defs, touch_count);
        }
        ph_launcher_notice(launcher, "Settings restored to defaults.");
        return ph_launch(PH_LAUNCH_SETTING, PH_SETTINGS_ALL);
    case PH_DO_BINDING:
        ph_input_capture(launcher->input, launcher->player, result.argument, result.column);
        return ph_launch(PH_LAUNCH_NONE, 0);
    case PH_DO_RESET_BINDINGS:
        ph_input_reset(launcher->input, launcher->player);
        return ph_launch(PH_LAUNCH_SAVE_CONFIG, 0);
    case PH_DO_ARRANGE:
        launcher->arranging = 1;
        ph_touch_edit(launcher->touch, 1);
        return ph_launch(PH_LAUNCH_NONE, 0);
    case PH_DO_RESET_LAYOUT:
        ph_touch_reset_layout(launcher->touch);
        return ph_launch(PH_LAUNCH_SAVE_CONFIG, 0);
    case PH_DO_PICK:
        if (launcher->pick_rom) {
            launcher->pick_rom(launcher, launcher->userdata);
        } else {
            ph_pick_file(launcher->window, launcher->rom_description, launcher->rom_patterns);
        }
        return ph_launch(PH_LAUNCH_NONE, 0);
    case PH_DO_REMOVE:
        ph_remove_import(launcher->rom_import);
        ph_launcher_rom_clear(launcher);
        ph_launcher_notice(launcher, "Removed the imported ROM.");
        return ph_launch(PH_LAUNCH_ROM_RESCAN, 0);
    case PH_DO_HOST:
        return ph_launcher_connect(launcher, ph_net_host(launcher->net));
    case PH_DO_GAME:
        return ph_launcher_connect(launcher, ph_net_join_game(launcher->net, result.argument));
    case PH_DO_CANCEL:
        return ph_launcher_back(launcher);
    case PH_DO_CUSTOM:
        if (launcher->custom_action) {
            return launcher->custom_action(launcher, result, launcher->userdata);
        }
        return ph_launch(PH_LAUNCH_CUSTOM, result.argument);
    default:
        return ph_launch(PH_LAUNCH_NONE, 0);
    }
}

static struct ph_launch ph_launcher_result(struct ph_launcher *launcher, struct ph_menu_result result)
{
    if (result.id == PH_DO_CUSTOM && launcher->custom_action &&
        (result.kind == PH_MENU_ACTIVATE || result.kind == PH_MENU_ADJUST || result.kind == PH_MENU_CLEAR)) {
        return launcher->custom_action(launcher, result, launcher->userdata);
    }
    switch (result.kind) {
    case PH_MENU_ACTIVATE:
        return ph_launcher_activate(launcher, result);
    case PH_MENU_ADJUST:
        return ph_launcher_adjust(launcher, result.id, result.argument, result.delta, 0);
    case PH_MENU_CLEAR:
        if (result.id == PH_DO_BINDING) {
            ph_input_bind(launcher->input, launcher->player, result.argument, result.column, (struct ph_binding){ PH_BIND_NONE, 0, 0 });
            return ph_launch(PH_LAUNCH_SAVE_CONFIG, 0);
        }
        return ph_launch(PH_LAUNCH_NONE, 0);
    case PH_MENU_BACK:
        return ph_launcher_back(launcher);
    case PH_MENU_EDITED:
        return ph_launcher_finish_edit(launcher, result.id);
    default:
        return ph_launch(PH_LAUNCH_NONE, 0);
    }
}

static struct ph_launch ph_launcher_arrange_event(struct ph_launcher *launcher, SDL_Renderer *renderer, const SDL_Event *event)
{
    int result = ph_touch_event(launcher->touch, renderer, event);
    if (result == PH_TOUCH_EDIT_DONE) {
        launcher->arranging = 0;
        return ph_launch(PH_LAUNCH_SAVE_CONFIG, 0);
    }
    return ph_launch(result == PH_TOUCH_EDITED ? PH_LAUNCH_SAVE_CONFIG : PH_LAUNCH_NONE, 0);
}

static struct ph_launch ph_launcher_capture_event(struct ph_launcher *launcher, const SDL_Event *event)
{
    int result = ph_input_event(launcher->input, event);
    return ph_launch(result == PH_INPUT_CHANGED ? PH_LAUNCH_SAVE_CONFIG : PH_LAUNCH_NONE, 0);
}

struct ph_launch ph_launcher_event(struct ph_launcher *launcher, SDL_Renderer *renderer, const SDL_Event *event)
{
    if (ph_files_event(event) == PH_FILES_LOOK_AGAIN) {
        return ph_launch(PH_LAUNCH_ROM_RESCAN, 0);
    }
    if (!launcher->open) {
        if (launcher->input) {
            ph_input_event(launcher->input, event);
        }
        if (launcher->touch) {
            ph_touch_event(launcher->touch, renderer, event);
        }
        return ph_launcher_pauses(launcher, event) ? ph_launcher_pause(launcher) : ph_launch(PH_LAUNCH_NONE, 0);
    }
    if (launcher->touch && event->type == SDL_EVENT_FINGER_DOWN) {
        launcher->touch->seen = 1;
    }
    if (launcher->arranging && launcher->touch) {
        return ph_launcher_arrange_event(launcher, renderer, event);
    }
    if (launcher->input && launcher->input->capturing) {
        return ph_launcher_capture_event(launcher, event);
    }
    if (launcher->input) {
        ph_input_event(launcher->input, event);
    }
    ph_launcher_build(launcher);
    return ph_launcher_result(launcher, ph_menu_event(&launcher->menu, renderer, event));
}

static struct ph_launch ph_launcher_lan_ended(struct ph_launcher *launcher)
{
    char message[PH_NET_MESSAGE];
    SDL_strlcpy(message, launcher->net->message, sizeof message);
    ph_net_stop(launcher->net);
    ph_launcher_open(launcher, PH_PAGE_MAIN);
    ph_launcher_notice(launcher, message[0] ? message : "The LAN game ended.");
    return ph_launch(PH_LAUNCH_END, 0);
}

struct ph_launch ph_launcher_update(struct ph_launcher *launcher)
{
    if (ph_take_pick(launcher->picked, sizeof launcher->picked)) {
        if (!ph_launcher_in_game(launcher)) {
            return ph_launch(PH_LAUNCH_ROM_PICKED, 0);
        }
        ph_launcher_notice(launcher, "Quit to the launcher to change the ROM.");
    }
    struct ph_net *net = launcher->net;
    if (net && ph_net_playing(net) && net->status != PH_NET_CONNECTED) {
        return ph_launcher_lan_ended(launcher);
    }
    if (net && ph_net_stalled(net)) {
        ph_launcher_notice(launcher, "Waiting for the other player...");
    }
    if (!launcher->open && launcher->touch && ph_touch_paused(launcher->touch)) {
        return ph_launcher_pause(launcher);
    }
    int page = launcher->menu.page;
    if (!launcher->open || !net || (page != PH_PAGE_LAN && page != PH_PAGE_WAITING)) {
        return ph_launch(PH_LAUNCH_NONE, 0);
    }
    ph_net_update(net);
    if (page == PH_PAGE_WAITING && net->status == PH_NET_CONNECTED) {
        ph_net_stop_search(net);
        return ph_launch(PH_LAUNCH_NETPLAY, net->local_player);
    }
    if (page == PH_PAGE_WAITING && net->status == PH_NET_FAILED) {
        ph_launcher_notice(launcher, net->message);
        ph_menu_pop(&launcher->menu);
        ph_net_search(net);
    }
    return ph_launch(PH_LAUNCH_NONE, 0);
}

void ph_launcher_draw(struct ph_launcher *launcher, SDL_Renderer *renderer)
{
    if (!launcher->open) {
        ph_menu_draw_notice_alone(&launcher->menu, renderer);
        return;
    }
    if (launcher->arranging && launcher->touch) {
        ph_touch_draw(launcher->touch, renderer);
        return;
    }
    ph_launcher_build(launcher);
    ph_menu_draw(&launcher->menu, renderer);
    const struct ph_input *input = launcher->input;
    if (input && input->capturing) {
        char strong[PH_MENU_TEXT];
        SDL_snprintf(strong, sizeof strong, "Player %d  %s", input->capture_player + 1, input->action_labels[input->capture_action]);
        const char *line = input->capture_slot == PH_SLOT_KEY ? "Press a key for" : "Press a gamepad button for";
        ph_menu_prompt(&launcher->menu, renderer, line, strong, "Esc cancel    Del clear");
    }
}

#endif
