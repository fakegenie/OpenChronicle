#ifndef PH_MENU_H
#define PH_MENU_H

#include <SDL3/SDL.h>
#include <stdint.h>
#include "ph_settings.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    PH_MENU_ROWS = 64,
    PH_MENU_TEXT = 128,
    PH_MENU_PAGES = 64,
    PH_MENU_DEPTH = 8,
    PH_MENU_NOTICE = 160,
    PH_MENU_SECTIONS = 16,
};

enum {
    PH_ROW_ACTION,
    PH_ROW_VALUE,
    PH_ROW_PAIR,
    PH_ROW_HEADING,
    PH_ROW_NOTE,
    PH_ROW_GAP,
    PH_ROW_FILL,
};

enum {
    PH_STYLE_LIST,
    PH_STYLE_BIG,
};

enum {
    PH_TONE_NORMAL,
    PH_TONE_DIM,
    PH_TONE_STRONG,
    PH_TONE_WARNING,
    PH_TONE_ERROR,
};

enum {
    PH_NAV_NONE,
    PH_NAV_UP,
    PH_NAV_DOWN,
    PH_NAV_LEFT,
    PH_NAV_RIGHT,
    PH_NAV_OK,
    PH_NAV_BACK,
    PH_NAV_CLEAR,
    PH_NAV_PREV_SECTION,
    PH_NAV_NEXT_SECTION,
    PH_NAV_TOGGLE_PANE,
};

enum {
    PH_MENU_NONE,
    PH_MENU_ACTIVATE,
    PH_MENU_ADJUST,
    PH_MENU_CLEAR,
    PH_MENU_BACK,
    PH_MENU_EDITED,
    PH_MENU_EDIT_CANCELLED,
};

struct ph_menu_result {
    int kind;
    int id;
    int argument;
    int delta;
    int column;
};

struct ph_row {
    int kind;
    int id;
    int argument;
    int disabled;
    int tone;
    int has_slider;
    float slider;
    char label[PH_MENU_TEXT];
    char value[PH_MENU_TEXT];
    char other[PH_MENU_TEXT];
};

struct ph_menu_theme {
    SDL_FColor background;
    SDL_FColor shade;
    SDL_FColor panel;
    SDL_FColor text;
    SDL_FColor bright;
    SDL_FColor dim;
    SDL_FColor accent;
    SDL_FColor heading;
    SDL_FColor warning;
    SDL_FColor error;
    SDL_FColor tile_alt;
    SDL_FColor border;
    SDL_FColor outline;
    SDL_FColor pill_ink;
};

struct ph_menu_layer {
    int valid;
    int page;
    int style;
    int selected;
    int column;
    int count;
    float scroll;
    char title[PH_MENU_TEXT];
    struct ph_row rows[PH_MENU_ROWS];
    SDL_FRect panel;
    SDL_FRect list;
    SDL_FRect back;
    SDL_FPoint title_at;
    float title_size;
    float row_height;
    float text_size;
    float arrow_size;
    float content_height;
    SDL_FRect boxes[PH_MENU_ROWS];
    float sizes[PH_MENU_ROWS];
};

struct ph_menu {
    const struct ph_menu_theme *theme;
    SDL_Texture *background;
    SDL_Texture *logo;
    int over_game;
    int touch;
    int style;
    int page;
    int stack[PH_MENU_DEPTH];
    int depth;
    int selected[PH_MENU_PAGES];
    float scroll[PH_MENU_PAGES];
    int column;
    char title[PH_MENU_TEXT];
    struct ph_row rows[PH_MENU_ROWS];
    int count;
    float points;
    float unit;
    SDL_FPoint origin;
    float width;
    float height;
    SDL_FRect panel;
    SDL_FRect aside;
    SDL_FRect back;
    SDL_FRect list;
    SDL_FRect boxes[PH_MENU_ROWS];
    float sizes[PH_MENU_ROWS];
    SDL_FPoint title_at;
    float title_size;
    float row_height;
    float text_size;
    float arrow_size;
    float content_height;
    struct ph_menu_layer layers[PH_MENU_DEPTH];
    int pressing;
    int press_finger;
    SDL_FingerID finger;
    int press_row;
    int press_zone;
    int press_secondary;
    int press_back;
    int press_parent;
    int dragged;
    SDL_FPoint press_at;
    float press_scroll;
    int follow;
    int stick_latched[2];
    int editing;
    int edit_id;
    int edit_numeric;
    char edit_text[PH_MENU_TEXT];
    SDL_Window *edit_window;
    char notice[PH_MENU_NOTICE];
    uint64_t notice_until;
};

enum {
    PH_FLAVOR_PLAIN,
    PH_FLAVOR_MINT,
    PH_FLAVOR_STRAWBERRY,
    PH_FLAVOR_BANANA,
    PH_FLAVOR_PEANUT,
    PH_FLAVOR_COUNT,
};

extern int ph_menu_flavor;
extern const char *const ph_menu_flavor_names[PH_FLAVOR_COUNT];
extern const struct ph_menu_theme ph_menu_default_theme;
struct ph_menu_theme ph_theme_make(uint32_t tile_a, uint32_t tile_b, uint32_t panel, uint32_t border, uint32_t accent);

void ph_menu_reset_flavor(void);
struct ph_setting ph_menu_flavor_setting(int section);
int ph_menu_take(const char *key, const char *value);
void ph_menu_write_config(SDL_IOStream *file);

void ph_menu_init(struct ph_menu *menu, const struct ph_menu_theme *theme, int page);
void ph_menu_open(struct ph_menu *menu, int page);
void ph_menu_push(struct ph_menu *menu, int page);
int ph_menu_pop(struct ph_menu *menu);
void ph_menu_begin(struct ph_menu *menu, const char *title, int style);
struct ph_row *ph_menu_add(struct ph_menu *menu, int kind, int id, int argument, const char *label, const char *value);
void ph_menu_end(struct ph_menu *menu);
void ph_menu_snapshot_layer(struct ph_menu *menu, int level);
void ph_menu_layout(struct ph_menu *menu, SDL_Renderer *renderer);
SDL_FRect ph_menu_row_rect(const struct ph_menu *menu, int row);
int ph_menu_nav(struct ph_menu *menu, const SDL_Event *event);
struct ph_menu_result ph_menu_apply(struct ph_menu *menu, int nav);
struct ph_menu_result ph_menu_event(struct ph_menu *menu, SDL_Renderer *renderer, const SDL_Event *event);
void ph_menu_edit(struct ph_menu *menu, SDL_Window *window, int id, const char *text, int numeric);
void ph_menu_stop_editing(struct ph_menu *menu);
void ph_menu_notice(struct ph_menu *menu, const char *text);
void ph_menu_draw(struct ph_menu *menu, SDL_Renderer *renderer);
void ph_menu_draw_notice_alone(struct ph_menu *menu, SDL_Renderer *renderer);
void ph_menu_prompt(struct ph_menu *menu, SDL_Renderer *renderer, const char *line, const char *strong, const char *hint);

#ifdef __cplusplus
}
#endif

#endif

#if defined(PH_IMPLEMENTATION) && !defined(PH_MENU_IMPLEMENTED)
#define PH_MENU_IMPLEMENTED

#include "ph_draw.h"
#include "ph_view.h"

enum {
    PH_ZONE_BODY,
    PH_ZONE_LESS,
    PH_ZONE_MORE,
    PH_ZONE_FIRST,
    PH_ZONE_SECOND,
    PH_STICK_PRESS = 16000,
    PH_STICK_RELEASE = 8000,
    PH_NOTICE_MILLISECONDS = 3500,
};

enum {
    PH_CANVAS_HEIGHT = 360,
    PH_SMALL = 11,
    PH_HEADER_TEXT = 11,
    PH_BODY = 15,
    PH_CARD_RADIUS = 7,
    PH_PILL_RADIUS = 4,
};

struct ph_window_palette {
    SDL_FColor outline;
    SDL_FColor border;
    SDL_FColor interior;
    SDL_FColor tile_a;
    SDL_FColor tile_b;
    SDL_FColor pill_focus;
    SDL_FColor pill_active;
    SDL_FColor pill_ink;
    SDL_FColor text;
    SDL_FColor dim;
};

int ph_menu_flavor = PH_FLAVOR_PLAIN;

const char *const ph_menu_flavor_names[PH_FLAVOR_COUNT] = {
    "Plain",
    "Mint",
    "Strawberry",
    "Banana",
    "Peanut",
};

const struct ph_menu_theme ph_menu_default_theme = {
    { 0.224f, 0.188f, 0.416f, 1.00f }, /* background (#39306A) */
    { 0.106f, 0.106f, 0.180f, 0.72f }, /* shade (#1B1B2E) */
    { 0.106f, 0.106f, 0.180f, 1.00f }, /* panel (#1B1B2E) */
    { 0.949f, 0.937f, 0.894f, 1.00f }, /* text (#F2EFE4) */
    { 1.000f, 1.000f, 1.000f, 1.00f }, /* bright */
    { 0.569f, 0.561f, 0.639f, 1.00f }, /* dim (#918FA3) */
    { 0.976f, 0.827f, 0.314f, 1.00f }, /* accent (#F9D350) */
    { 0.949f, 0.937f, 0.894f, 1.00f }, /* heading (#F2EFE4) */
    { 0.976f, 0.827f, 0.314f, 1.00f }, /* warning (#F9D350) */
    { 0.949f, 0.447f, 0.420f, 1.00f }, /* error (#F2726B) */
    { 0.180f, 0.153f, 0.349f, 1.00f }, /* tile_alt (#2E2759) */
    { 0.949f, 0.937f, 0.894f, 1.00f }, /* border (#F2EFE4) */
    { 0.106f, 0.106f, 0.180f, 1.00f }, /* outline (#1B1B2E) */
    { 0.106f, 0.106f, 0.180f, 1.00f }, /* pill_ink (#1B1B2E) */
};

struct ph_menu_theme ph_theme_make(uint32_t tile_a, uint32_t tile_b, uint32_t panel, uint32_t border, uint32_t accent)
{
    SDL_FColor c_panel = ph_color(panel, 1.0f);
    SDL_FColor c_border = ph_color(border, 1.0f);
    SDL_FColor c_accent = ph_color(accent, 1.0f);
    SDL_FColor c_dim = {
        c_panel.r * 0.42f + c_border.r * 0.58f,
        c_panel.g * 0.42f + c_border.g * 0.58f,
        c_panel.b * 0.42f + c_border.b * 0.58f,
        1.0f
    };
    float accent_luma = c_accent.r * 0.299f + c_accent.g * 0.587f + c_accent.b * 0.114f;
    struct ph_menu_theme t;
    t.background = ph_color(tile_a, 1.0f);
    t.shade = ph_color(panel, 0.74f);
    t.panel = c_panel;
    t.text = c_border;
    t.bright = (SDL_FColor){ 1.0f, 1.0f, 1.0f, 1.0f };
    t.dim = c_dim;
    t.accent = c_accent;
    t.heading = c_border;
    t.warning = c_accent;
    t.error = ph_color(0xF2726B, 1.0f);
    t.tile_alt = ph_color(tile_b, 1.0f);
    t.border = c_border;
    t.outline = c_panel;
    t.pill_ink = accent_luma < 0.44f ? c_border : c_panel;
    return t;
}

static const struct ph_window_palette ph_window_palettes[PH_FLAVOR_COUNT] = {
    /* Plain */
    {
        { 0.106f, 0.106f, 0.180f, 1.00f },
        { 0.949f, 0.937f, 0.894f, 1.00f },
        { 0.106f, 0.106f, 0.180f, 1.00f },
        { 0.224f, 0.188f, 0.416f, 1.00f },
        { 0.180f, 0.153f, 0.349f, 1.00f },
        { 0.976f, 0.827f, 0.314f, 1.00f },
        { 0.949f, 0.937f, 0.894f, 1.00f },
        { 0.106f, 0.106f, 0.180f, 1.00f },
        { 0.949f, 0.937f, 0.894f, 1.00f },
        { 0.569f, 0.561f, 0.639f, 1.00f },
    },
    /* Mint */
    {
        { 0.075f, 0.145f, 0.161f, 1.00f },
        { 0.808f, 0.961f, 0.894f, 1.00f },
        { 0.075f, 0.145f, 0.161f, 1.00f },
        { 0.141f, 0.286f, 0.310f, 1.00f },
        { 0.110f, 0.231f, 0.255f, 1.00f },
        { 0.490f, 0.918f, 0.745f, 1.00f },
        { 0.808f, 0.961f, 0.894f, 1.00f },
        { 0.075f, 0.145f, 0.161f, 1.00f },
        { 0.898f, 0.976f, 0.941f, 1.00f },
        { 0.506f, 0.682f, 0.659f, 1.00f },
    },
    /* Strawberry */
    {
        { 0.169f, 0.090f, 0.137f, 1.00f },
        { 0.988f, 0.847f, 0.890f, 1.00f },
        { 0.169f, 0.090f, 0.137f, 1.00f },
        { 0.349f, 0.184f, 0.278f, 1.00f },
        { 0.286f, 0.149f, 0.231f, 1.00f },
        { 1.000f, 0.635f, 0.753f, 1.00f },
        { 0.988f, 0.847f, 0.890f, 1.00f },
        { 0.169f, 0.090f, 0.137f, 1.00f },
        { 0.988f, 0.914f, 0.937f, 1.00f },
        { 0.698f, 0.533f, 0.608f, 1.00f },
    },
    /* Banana */
    {
        { 0.157f, 0.125f, 0.082f, 1.00f },
        { 0.988f, 0.945f, 0.784f, 1.00f },
        { 0.157f, 0.125f, 0.082f, 1.00f },
        { 0.322f, 0.259f, 0.165f, 1.00f },
        { 0.263f, 0.212f, 0.133f, 1.00f },
        { 0.988f, 0.843f, 0.329f, 1.00f },
        { 0.988f, 0.945f, 0.784f, 1.00f },
        { 0.157f, 0.125f, 0.082f, 1.00f },
        { 0.988f, 0.961f, 0.878f, 1.00f },
        { 0.682f, 0.627f, 0.498f, 1.00f },
    },
    /* Peanut */
    {
        { 0.161f, 0.114f, 0.086f, 1.00f },
        { 0.949f, 0.871f, 0.776f, 1.00f },
        { 0.161f, 0.114f, 0.086f, 1.00f },
        { 0.318f, 0.224f, 0.169f, 1.00f },
        { 0.259f, 0.180f, 0.137f, 1.00f },
        { 0.937f, 0.698f, 0.431f, 1.00f },
        { 0.949f, 0.871f, 0.776f, 1.00f },
        { 0.161f, 0.114f, 0.086f, 1.00f },
        { 0.965f, 0.918f, 0.863f, 1.00f },
        { 0.667f, 0.576f, 0.506f, 1.00f },
    },
};

void ph_menu_reset_flavor(void)
{
    ph_menu_flavor = PH_FLAVOR_PLAIN;
}

struct ph_setting ph_menu_flavor_setting(int section)
{
    struct ph_setting setting;
    SDL_zero(setting);
    setting.key = "window_flavor";
    setting.label = "Window flavor";
    setting.value = &ph_menu_flavor;
    setting.fallback = PH_FLAVOR_PLAIN;
    setting.choices = ph_menu_flavor_names;
    setting.choice_count = PH_FLAVOR_COUNT;
    setting.section = section;
    return setting;
}

int ph_menu_take(const char *key, const char *value)
{
    struct ph_setting setting = ph_menu_flavor_setting(0);
    return ph_settings_take(&setting, 1, key, value);
}

void ph_menu_write_config(SDL_IOStream *file)
{
    struct ph_setting setting = ph_menu_flavor_setting(0);
    ph_settings_write(file, &setting, 1);
}

static struct ph_window_palette ph_menu_palette(const struct ph_menu *menu)
{
    int flavor = SDL_clamp(ph_menu_flavor, 0, PH_FLAVOR_COUNT - 1);
    if (flavor != PH_FLAVOR_PLAIN && (!menu->theme || menu->theme == &ph_menu_default_theme)) {
        return ph_window_palettes[flavor];
    }
    const struct ph_menu_theme *t = menu->theme ? menu->theme : &ph_menu_default_theme;
    struct ph_window_palette custom;
    SDL_FColor opaque_panel = { t->panel.r, t->panel.g, t->panel.b, 1.0f };
    custom.interior = opaque_panel;
    custom.outline = t->outline.a > 0.0f ? t->outline : opaque_panel;
    custom.border = t->border.a > 0.0f ? t->border : t->text;
    custom.tile_a = t->background;
    custom.tile_b = t->tile_alt.a > 0.0f ? t->tile_alt : (SDL_FColor){ t->background.r * 0.82f, t->background.g * 0.82f, t->background.b * 0.82f, 1.0f };
    custom.pill_focus = t->accent;
    custom.pill_active = custom.border;
    custom.pill_ink = t->pill_ink.a > 0.0f ? t->pill_ink : opaque_panel;
    custom.text = t->text;
    custom.dim = t->dim;
    return custom;
}

static void ph_menu_reset_pointer(struct ph_menu *menu)
{
    menu->pressing = 0;
    menu->press_row = -1;
    menu->press_back = 0;
    menu->press_parent = 0;
    menu->dragged = 0;
}

static void ph_menu_activate_page(struct ph_menu *menu, int page)
{
    page = SDL_clamp(page, 0, PH_MENU_PAGES - 1);
    ph_menu_stop_editing(menu);
    ph_menu_reset_pointer(menu);
    menu->page = page;
    menu->depth = SDL_clamp(menu->depth, 1, PH_MENU_DEPTH);
    menu->stack[menu->depth - 1] = page;
    menu->follow = 1;
}

void ph_menu_init(struct ph_menu *menu, const struct ph_menu_theme *theme, int page)
{
    SDL_zerop(menu);
    menu->theme = theme ? theme : &ph_menu_default_theme;
    menu->points = 1;
    menu->unit = 1;
    ph_menu_open(menu, page);
}

void ph_menu_open(struct ph_menu *menu, int page)
{
    menu->depth = 1;
    SDL_zero(menu->layers);
    ph_menu_activate_page(menu, page);
}

void ph_menu_push(struct ph_menu *menu, int page)
{
    if (menu->depth < PH_MENU_DEPTH) {
        menu->depth++;
    }
    menu->layers[menu->depth - 1].valid = 0;
    ph_menu_activate_page(menu, page);
}

int ph_menu_pop(struct ph_menu *menu)
{
    if (menu->depth <= 1) {
        return 0;
    }
    menu->layers[menu->depth - 1].valid = 0;
    menu->depth--;
    ph_menu_activate_page(menu, menu->stack[menu->depth - 1]);
    return 1;
}

void ph_menu_begin(struct ph_menu *menu, const char *title, int style)
{
    menu->count = 0;
    menu->style = style;
    SDL_strlcpy(menu->title, title ? title : "", sizeof menu->title);
}

struct ph_row *ph_menu_add(struct ph_menu *menu, int kind, int id, int argument, const char *label, const char *value)
{
    if (menu->count >= PH_MENU_ROWS) {
        return NULL;
    }
    struct ph_row *row = &menu->rows[menu->count++];
    SDL_zerop(row);
    row->kind = kind;
    row->id = id;
    row->argument = argument;
    SDL_strlcpy(row->label, label ? label : "", sizeof row->label);
    SDL_strlcpy(row->value, value ? value : "", sizeof row->value);
    return row;
}

static int ph_menu_row_selectable(const struct ph_row *rows, int count, int index)
{
    if (index < 0 || index >= count || rows[index].disabled) {
        return 0;
    }
    int kind = rows[index].kind;
    return kind == PH_ROW_ACTION || kind == PH_ROW_VALUE || kind == PH_ROW_PAIR;
}

static int ph_menu_selectable(const struct ph_menu *menu, int index)
{
    return ph_menu_row_selectable(menu->rows, menu->count, index);
}

static void ph_menu_settle(struct ph_menu *menu, int step)
{
    int *selected = &menu->selected[menu->page];
    if (!menu->count) {
        *selected = 0;
        return;
    }
    *selected = SDL_clamp(*selected, 0, menu->count - 1);
    for (int tries = 0; tries < menu->count && !ph_menu_selectable(menu, *selected); tries++) {
        *selected = (*selected + step + menu->count) % menu->count;
    }
}

void ph_menu_snapshot_layer(struct ph_menu *menu, int level)
{
    if (level < 0 || level >= PH_MENU_DEPTH) {
        return;
    }
    struct ph_menu_layer *layer = &menu->layers[level];
    layer->valid = 1;
    layer->page = menu->page;
    layer->style = menu->style;
    layer->selected = menu->selected[menu->page];
    layer->column = menu->column;
    layer->count = menu->count;
    layer->scroll = menu->scroll[menu->page];
    SDL_strlcpy(layer->title, menu->title, sizeof layer->title);
    if (menu->count > 0) {
        SDL_memcpy(layer->rows, menu->rows, (size_t)menu->count * sizeof menu->rows[0]);
    }
}

void ph_menu_end(struct ph_menu *menu)
{
    ph_menu_settle(menu, 1);
    int level = SDL_clamp(menu->depth - 1, 0, PH_MENU_DEPTH - 1);
    ph_menu_snapshot_layer(menu, level);
}

static SDL_FRect ph_menu_pixels(const struct ph_menu *menu, SDL_FRect area)
{
    float x0 = SDL_roundf(menu->origin.x + area.x * menu->unit);
    float y0 = SDL_roundf(menu->origin.y + area.y * menu->unit);
    float x1 = SDL_roundf(menu->origin.x + (area.x + area.w) * menu->unit);
    float y1 = SDL_roundf(menu->origin.y + (area.y + area.h) * menu->unit);
    return (SDL_FRect){ x0, y0, SDL_max(0.0f, x1 - x0), SDL_max(0.0f, y1 - y0) };
}

static float ph_menu_body_size(const struct ph_menu *menu)
{
    if (menu->touch) {
        return 15.0f;
    }
    if (menu->unit < 0.9f) {
        return 13.0f / menu->unit;
    }
    if (menu->unit < 1.55f) {
        return 22.0f / menu->unit;
    }
    return (float)PH_BODY;
}

static float ph_menu_header_size(const struct ph_menu *menu)
{
    if (menu->unit < 0.9f) {
        return 13.0f / menu->unit;
    }
    if (menu->unit < 1.55f) {
        return 14.0f / menu->unit;
    }
    return (float)PH_HEADER_TEXT;
}

static SDL_FPoint ph_menu_units(const struct ph_menu *menu, SDL_FPoint point)
{
    return (SDL_FPoint){ (point.x - menu->origin.x) / menu->unit, (point.y - menu->origin.y) / menu->unit };
}

static int ph_menu_inside(SDL_FRect area, SDL_FPoint point)
{
    return area.w > 0 && area.h > 0 && point.x >= area.x && point.x < area.x + area.w && point.y >= area.y && point.y < area.y + area.h;
}

static float ph_menu_measure(const struct ph_menu *menu, const char *text, float size, int weight)
{
    return ph_text_width(text, size * menu->unit, weight) / menu->unit;
}

static float ph_menu_measure_tracked(const struct ph_menu *menu, const char *text, float size, float tracking)
{
    float total = 0;
    char ch[2] = { 0, 0 };
    for (const char *p = text; *p; p++) {
        char c = (*p >= 'a' && *p <= 'z') ? (char)(*p - 32) : *p;
        ch[0] = c;
        total += ph_menu_measure(menu, ch, size, PH_TEXT_MEDIUM);
        if (p[1] && menu->unit >= 0.9f) {
            total += tracking;
        }
    }
    return total;
}

static int ph_menu_line(const struct ph_menu *menu, const char *text, float size, int weight, float width)
{
    char line[PH_MENU_TEXT];
    int fits = 0;
    int end = 0;
    while (text[end] && end < (int)sizeof line - 1) {
        while (text[end] && text[end] != ' ' && end < (int)sizeof line - 1) {
            end++;
        }
        SDL_strlcpy(line, text, (size_t)end + 1);
        if (fits && ph_menu_measure(menu, line, size, weight) > width) {
            return fits;
        }
        fits = end;
        while (text[end] == ' ') {
            end++;
        }
    }
    return SDL_max(fits, end);
}

static int ph_menu_lines(const struct ph_menu *menu, const char *text, float size, int weight, float width)
{
    int lines = 0;
    while (*text) {
        text += ph_menu_line(menu, text, size, weight, width);
        while (*text == ' ') {
            text++;
        }
        lines++;
    }
    return SDL_max(lines, 1);
}

static float ph_menu_tone_size(int tone)
{
    switch (tone) {
    case PH_TONE_STRONG:
        return 12;
    case PH_TONE_WARNING:
    case PH_TONE_ERROR:
        return PH_SMALL;
    default:
        return PH_BODY;
    }
}

static int ph_menu_tone_weight(int tone)
{
    return tone == PH_TONE_STRONG ? PH_TEXT_BOLD : PH_TEXT_MEDIUM;
}

static SDL_FColor ph_menu_tone_color(const struct ph_menu_theme *theme, int tone, SDL_FColor normal)
{
    switch (tone) {
    case PH_TONE_DIM:
        return theme->dim;
    case PH_TONE_STRONG:
        return theme->accent;
    case PH_TONE_WARNING:
        return theme->warning;
    case PH_TONE_ERROR:
        return theme->error;
    default:
        return normal;
    }
}

static int ph_menu_item(const struct ph_row *row)
{
    return row->kind == PH_ROW_ACTION || row->kind == PH_ROW_VALUE || row->kind == PH_ROW_PAIR;
}

static int ph_menu_spacer(const struct ph_row *row)
{
    return row->kind == PH_ROW_GAP || row->kind == PH_ROW_FILL;
}

static float ph_menu_note_size(const struct ph_menu *menu, const struct ph_row *row)
{
    if (row->kind == PH_ROW_HEADING) {
        return ph_menu_header_size(menu);
    }
    float base = ph_menu_tone_size(row->tone);
    if (menu->unit < 0.9f) {
        return SDL_max(base, 13.0f / menu->unit);
    }
    if (menu->unit < 1.55f && base >= (float)PH_BODY) {
        return 22.0f / menu->unit;
    }
    if (menu->unit < 1.55f) {
        return SDL_max(base, 14.0f / menu->unit);
    }
    return base;
}

static int ph_menu_note_weight(const struct ph_row *row)
{
    return row->kind == PH_ROW_HEADING ? PH_TEXT_BOLD : ph_menu_tone_weight(row->tone);
}

static const char *ph_menu_layer_caption(const struct ph_menu *menu, const struct ph_menu_layer *layer, int level)
{
    if (layer->title[0]) {
        return layer->title;
    }
    if (level == 0 && menu->over_game) {
        return "Paused";
    }
    return "";
}

static void ph_menu_layout_card(struct ph_menu *menu, struct ph_menu_layer *layer, int level, float prev_x, float prev_y, int use_aside)
{
    int low_res = menu->unit < 0.9f;
    int compact = menu->width <= 500.0f;
    float mx = low_res ? 12.0f : (compact ? 16.0f : 22.0f);
    float my = low_res ? 12.0f : (compact ? 16.0f : 22.0f);
    float step_x = low_res ? 32.0f : (compact ? 46.0f : 56.0f);
    float step_y = low_res ? 14.0f : (compact ? 18.0f : 22.0f);

    float hdr_sz = ph_menu_header_size(menu);
    float text_sz = ph_menu_body_size(menu);
    float row_h = menu->touch ? 30.0f : (low_res ? 25.0f : 26.0f);
    float row_step = row_h + (low_res ? 3.0f : 4.0f);
    layer->title_size = hdr_sz;
    layer->text_size = text_sz;
    layer->row_height = row_h;
    layer->arrow_size = menu->touch ? 16.0f : 11.0f;

    const char *caption = ph_menu_layer_caption(menu, layer, level);
    int has_header = caption[0] != 0 || level > 0;
    float header_h = has_header ? (low_res ? 26.0f : 29.0f) : (low_res ? 8.0f : 10.0f);
    float pad_x = low_res ? 8.0f : 10.0f;
    float pad_bottom = low_res ? 8.0f : 10.0f;

    int has_pair = 0;
    int has_value_ctrl = 0;
    int has_right_text = 0;
    int has_inline_notes = 0;
    int has_aside_notes = 0;
    float max_label_w = 0.0f;

    for (int i = 0; i < layer->count; i++) {
        const struct ph_row *row = &layer->rows[i];
        if (ph_menu_spacer(row)) {
            continue;
        }
        if (row->kind == PH_ROW_PAIR || (row->kind == PH_ROW_HEADING && row->other[0])) {
            has_pair = 1;
        } else if (row->kind == PH_ROW_VALUE) {
            has_value_ctrl = 1;
        } else if (row->kind == PH_ROW_ACTION) {
            float lw = ph_menu_measure(menu, row->label, text_sz, PH_TEXT_MEDIUM);
            if (lw > max_label_w) {
                max_label_w = lw;
            }
            if (row->value[0]) {
                has_right_text = 1;
                float vw = lw + ph_menu_measure(menu, row->value, text_sz * 0.85f, PH_TEXT_MEDIUM) + 26.0f;
                if (vw > max_label_w) {
                    max_label_w = vw;
                }
            }
        } else if (!ph_menu_item(row)) {
            if (use_aside) {
                has_aside_notes = 1;
            } else {
                has_inline_notes = 1;
            }
        }
    }

    float title_w = caption[0] ? ph_menu_measure_tracked(menu, caption, hdr_sz, 0.9f) + (level > 0 ? 12.0f : 0.0f) : 0.0f;
    float target_w;
    if (has_pair) {
        target_w = compact ? 336.0f : 372.0f;
    } else if (has_value_ctrl) {
        target_w = compact ? 286.0f : 326.0f;
    } else if (has_right_text || has_inline_notes) {
        target_w = SDL_clamp(SDL_max(max_label_w + 48.0f, compact ? 248.0f : 284.0f), 220.0f, compact ? 292.0f : 332.0f);
    } else {
        float content_w = SDL_max(max_label_w + 48.0f, title_w + 38.0f);
        target_w = SDL_clamp(content_w, compact ? 168.0f : 182.0f, compact ? 228.0f : 256.0f);
    }
    float card_w = SDL_min(target_w, menu->width - mx * 2.0f);

    float temp_y = has_header ? 2.0f : 0.0f;
    for (int i = 0; i < layer->count; i++) {
        const struct ph_row *row = &layer->rows[i];
        if (row->kind == PH_ROW_FILL) {
            layer->boxes[i] = (SDL_FRect){ pad_x, temp_y, card_w - pad_x * 2.0f, 0.0f };
            layer->sizes[i] = 0.0f;
        } else if (row->kind == PH_ROW_GAP) {
            layer->boxes[i] = (SDL_FRect){ pad_x, temp_y, card_w - pad_x * 2.0f, 6.0f };
            layer->sizes[i] = 0.0f;
            temp_y += 6.0f;
        } else if (!ph_menu_item(row) && use_aside) {
            layer->boxes[i] = (SDL_FRect){ 0.0f, 0.0f, 0.0f, 0.0f };
            layer->sizes[i] = ph_menu_note_size(menu, row);
        } else if (row->kind == PH_ROW_HEADING) {
            if (temp_y > 4.0f) {
                temp_y += 4.0f;
            }
            float hh = SDL_max(18.0f, hdr_sz + 4.0f);
            layer->boxes[i] = (SDL_FRect){ pad_x, temp_y, card_w - pad_x * 2.0f, hh };
            layer->sizes[i] = hdr_sz;
            temp_y += hh + 2.0f;
        } else if (!ph_menu_item(row)) {
            float sz = ph_menu_note_size(menu, row);
            float line_h = sz * (low_res ? 1.2f : 1.32f);
            float nh = ph_menu_lines(menu, row->label, sz, ph_menu_note_weight(row), card_w - pad_x * 2.0f - 10.0f) * line_h + 3.0f;
            layer->boxes[i] = (SDL_FRect){ pad_x, temp_y, card_w - pad_x * 2.0f, nh };
            layer->sizes[i] = sz;
            temp_y += nh + 4.0f;
        } else {
            layer->boxes[i] = (SDL_FRect){ pad_x, temp_y, card_w - pad_x * 2.0f, row_h };
            layer->sizes[i] = text_sz;
            temp_y += row_step;
        }
    }
    if (temp_y > row_h) {
        temp_y -= (low_res ? 3.0f : 4.0f);
    }

    float raw_list_h = SDL_max(temp_y, row_h);
    float raw_card_h = header_h + raw_list_h + pad_bottom;

    float card_x, card_y;
    if (level == 0) {
        card_x = mx;
        card_y = my;
    } else {
        float want_x = prev_x + step_x;
        float want_y = prev_y + step_y;
        card_x = SDL_min(want_x, SDL_max(mx, menu->width - mx - card_w));
        float min_step_y = low_res ? 10.0f : 14.0f;
        float min_y = SDL_min(prev_y + min_step_y, SDL_max(my, menu->height - my - 96.0f));
        float fit_y = menu->height - my - raw_card_h;
        card_y = fit_y < want_y ? SDL_max(min_y, fit_y) : want_y;
    }

    float max_card_h = SDL_max(96.0f, menu->height - card_y - my);
    float max_list_h = SDL_max(row_h, max_card_h - header_h - pad_bottom);
    float visible_list_h = SDL_min(raw_list_h, max_list_h);
    float card_h = header_h + visible_list_h + pad_bottom;
    float list_top = card_y + header_h;

    for (int i = 0; i < layer->count; i++) {
        if (!use_aside || ph_menu_item(&layer->rows[i]) || ph_menu_spacer(&layer->rows[i])) {
            layer->boxes[i].x += card_x;
            layer->boxes[i].y += list_top;
        }
    }

    layer->panel = (SDL_FRect){ card_x, card_y, card_w, card_h };
    layer->list = (SDL_FRect){ card_x + 3.0f, list_top, card_w - 6.0f, visible_list_h };
    layer->content_height = raw_list_h;
    layer->title_at = (SDL_FPoint){ card_x + (low_res ? 12.0f : 14.0f), card_y + (header_h - hdr_sz) * 0.5f };
    layer->back = (SDL_FRect){ 0.0f, 0.0f, 0.0f, 0.0f };
    if (level > 0) {
        layer->back = (SDL_FRect){ card_x + 5.0f, card_y + 3.0f, SDL_min(card_w - 10.0f, SDL_max(96.0f, title_w + 22.0f)), header_h - 4.0f };
    }

    if (use_aside && has_aside_notes) {
        float avail_right = menu->width - (card_x + card_w + 12.0f) - mx;
        float max_aside_w = SDL_clamp(avail_right, 130.0f, compact ? 256.0f : 316.0f);
        float longest = 0.0f;
        for (int i = 0; i < layer->count; i++) {
            const struct ph_row *row = &layer->rows[i];
            if (!ph_menu_item(row) && !ph_menu_spacer(row)) {
                float w = ph_menu_measure(menu, row->label, layer->sizes[i], ph_menu_note_weight(row));
                if (w > longest) {
                    longest = w;
                }
            }
        }
        float aside_w = SDL_clamp(longest + pad_x * 2.0f + 18.0f, 136.0f, max_aside_w);
        float inner_w = aside_w - pad_x * 2.0f - 8.0f;
        float ay = 0.0f;
        for (int i = 0; i < layer->count; i++) {
            const struct ph_row *row = &layer->rows[i];
            if (!ph_menu_item(row) && !ph_menu_spacer(row)) {
                float sz = layer->sizes[i];
                float line_h = sz * (low_res ? 1.2f : 1.32f);
                float nh = ph_menu_lines(menu, row->label, sz, ph_menu_note_weight(row), inner_w) * line_h;
                layer->boxes[i] = (SDL_FRect){ 0.0f, ay, inner_w, nh };
                ay += nh + 4.0f;
            }
        }
        if (ay > 4.0f) {
            ay -= 4.0f;
        }
        float aside_pad_y = low_res ? 9.0f : 11.0f;
        float aside_h = ay + aside_pad_y * 2.0f;
        float aside_x = menu->width - mx - aside_w;
        float aside_y = SDL_max(my, menu->height - my - aside_h);
        menu->aside = (SDL_FRect){ aside_x, aside_y, aside_w, aside_h };
        for (int i = 0; i < layer->count; i++) {
            const struct ph_row *row = &layer->rows[i];
            if (!ph_menu_item(row) && !ph_menu_spacer(row)) {
                layer->boxes[i].x = aside_x + pad_x;
                layer->boxes[i].y += aside_y + aside_pad_y;
            }
        }
    }
}

static float ph_menu_layer_scroll_limit(const struct ph_menu_layer *layer)
{
    return SDL_max(0.0f, layer->content_height - layer->list.h);
}

static float ph_menu_scroll_limit(const struct ph_menu *menu)
{
    return SDL_max(0.0f, menu->content_height - menu->list.h);
}

static void ph_menu_clamp_scroll(struct ph_menu *menu)
{
    menu->scroll[menu->page] = SDL_clamp(menu->scroll[menu->page], 0.0f, ph_menu_scroll_limit(menu));
}

static void ph_menu_follow(struct ph_menu *menu)
{
    int selected = menu->selected[menu->page];
    if (!menu->count || ph_menu_scroll_limit(menu) <= 0.0f) {
        return;
    }
    SDL_FRect box = menu->boxes[selected];
    if (box.h <= 0) {
        return;
    }
    float top = box.y - menu->list.y - 4.0f;
    if (selected > 0 && menu->rows[selected - 1].kind == PH_ROW_HEADING) {
        top -= menu->boxes[selected - 1].h;
    }
    float bottom = box.y + box.h - menu->list.y + 4.0f;
    float *scroll = &menu->scroll[menu->page];
    *scroll = SDL_min(*scroll, top);
    *scroll = SDL_max(*scroll, bottom - menu->list.h);
}

void ph_menu_layout(struct ph_menu *menu, SDL_Renderer *renderer)
{
    SDL_FRect safe = ph_view_safe(renderer);
    float short_side = SDL_min(safe.w, safe.h);
    float unit = short_side / (float)PH_CANVAS_HEIGHT;
    menu->unit = unit > 0 ? unit : 1.0f;
    menu->points = ph_view_points(renderer);
    menu->origin = (SDL_FPoint){ safe.x, safe.y };
    menu->width = SDL_max(safe.w / menu->unit, 240.0f);
    menu->height = SDL_max(safe.h / menu->unit, (float)PH_CANVAS_HEIGHT);
    menu->aside = (SDL_FRect){ 0.0f, 0.0f, 0.0f, 0.0f };

    int low_res = menu->unit < 0.9f;
    int compact = menu->width <= 500.0f;
    float mx = low_res ? 12.0f : (compact ? 16.0f : 22.0f);
    float my = low_res ? 12.0f : (compact ? 16.0f : 22.0f);
    float step_x = low_res ? 24.0f : (compact ? 32.0f : 44.0f);
    float step_y = low_res ? 14.0f : (compact ? 18.0f : 22.0f);

    int active = SDL_clamp(menu->depth - 1, 0, PH_MENU_DEPTH - 1);
    ph_menu_snapshot_layer(menu, active);

    float prev_x = mx;
    float prev_y = my;
    for (int lvl = 0; lvl <= active; lvl++) {
        struct ph_menu_layer *layer = &menu->layers[lvl];
        if (!layer->valid) {
            if (lvl > 0) {
                prev_x += step_x;
                prev_y += step_y;
            }
            continue;
        }
        int use_aside = (lvl == 0 && !menu->over_game && !layer->title[0]);
        ph_menu_layout_card(menu, layer, lvl, prev_x, prev_y, use_aside);
        layer->scroll = SDL_clamp(layer->scroll, 0.0f, ph_menu_layer_scroll_limit(layer));
        prev_x = layer->panel.x;
        prev_y = layer->panel.y;
    }

    const struct ph_menu_layer *top = &menu->layers[active];
    menu->panel = top->panel;
    menu->list = top->list;
    menu->back = top->back;
    menu->title_at = top->title_at;
    menu->title_size = top->title_size;
    menu->row_height = top->row_height;
    menu->text_size = top->text_size;
    menu->arrow_size = top->arrow_size;
    menu->content_height = top->content_height;
    if (menu->count > 0) {
        SDL_memcpy(menu->boxes, top->boxes, (size_t)menu->count * sizeof top->boxes[0]);
        SDL_memcpy(menu->sizes, top->sizes, (size_t)menu->count * sizeof top->sizes[0]);
    }

    if (menu->follow) {
        menu->follow = 0;
        ph_menu_follow(menu);
    }
    ph_menu_clamp_scroll(menu);
    menu->layers[active].scroll = menu->scroll[menu->page];
    menu->layers[active].selected = menu->selected[menu->page];
    menu->layers[active].column = menu->column;
}

static int ph_menu_row_in_aside(const struct ph_menu *menu, int level, const struct ph_row *row)
{
    return level == 0 && !menu->over_game && !menu->layers[0].title[0] && !ph_menu_item(row) && !ph_menu_spacer(row) && menu->aside.w > 0.0f;
}

static SDL_FRect ph_menu_box(const struct ph_menu *menu, int row)
{
    SDL_FRect box = menu->boxes[row];
    int active = SDL_clamp(menu->depth - 1, 0, PH_MENU_DEPTH - 1);
    if (!ph_menu_row_in_aside(menu, active, &menu->rows[row]) && ph_menu_scroll_limit(menu) > 0.0f) {
        box.y -= menu->scroll[menu->page];
    }
    return box;
}

SDL_FRect ph_menu_row_rect(const struct ph_menu *menu, int row)
{
    return ph_menu_pixels(menu, ph_menu_box(menu, row));
}

static int ph_menu_parent_at(const struct ph_menu *menu, SDL_FPoint point)
{
    if (ph_menu_inside(menu->panel, point)) {
        return 0;
    }
    int active = SDL_clamp(menu->depth - 1, 0, PH_MENU_DEPTH - 1);
    for (int lvl = active - 1; lvl >= 0; lvl--) {
        if (menu->layers[lvl].valid && ph_menu_inside(menu->layers[lvl].panel, point)) {
            return 1;
        }
    }
    return 0;
}

static int ph_menu_row_at(const struct ph_menu *menu, SDL_FPoint point)
{
    if (!ph_menu_inside(menu->list, point)) {
        return -1;
    }
    for (int index = 0; index < menu->count; index++) {
        if (!ph_menu_spacer(&menu->rows[index])) {
            SDL_FRect box = ph_menu_box(menu, index);
            if (ph_menu_inside(box, point)) {
                return index;
            }
        }
    }
    return -1;
}

static void ph_menu_value_bounds(const struct ph_menu *menu, SDL_FRect box, float *less_x, float *more_x)
{
    float pad = menu->unit < 0.9f ? 6.0f : 10.0f;
    float right = box.x + box.w - pad;
    float span = SDL_clamp(box.w * 0.42f, 96.0f, 148.0f);
    *more_x = right;
    *less_x = right - span;
}

static void ph_menu_pair_columns(SDL_FRect box, float *col0, float *col1)
{
    *col0 = box.x + box.w * 0.44f;
    *col1 = box.x + box.w * 0.72f;
}

static int ph_menu_zone(const struct ph_menu *menu, int row, SDL_FPoint point)
{
    const struct ph_row *line = &menu->rows[row];
    SDL_FRect box = ph_menu_box(menu, row);
    if (line->kind == PH_ROW_VALUE) {
        float less_x = 0, more_x = 0;
        ph_menu_value_bounds(menu, box, &less_x, &more_x);
        float mid = (less_x + more_x) * 0.5f;
        if (line->has_slider && point.x >= less_x - 8.0f) {
            return point.x < mid ? PH_ZONE_LESS : PH_ZONE_MORE;
        }
        if (point.x >= less_x - 10.0f && point.x < less_x + 16.0f) {
            return PH_ZONE_LESS;
        }
        if (point.x >= more_x - 16.0f) {
            return PH_ZONE_MORE;
        }
        return PH_ZONE_BODY;
    }
    if (line->kind == PH_ROW_PAIR) {
        float col0 = 0, col1 = 0;
        ph_menu_pair_columns(box, &col0, &col1);
        return point.x >= col1 - 4.0f ? PH_ZONE_SECOND : point.x >= col0 - 4.0f ? PH_ZONE_FIRST : PH_ZONE_BODY;
    }
    return PH_ZONE_BODY;
}

static struct ph_menu_result ph_menu_result(const struct ph_menu *menu, int kind, int delta)
{
    const struct ph_row *row = menu->count ? &menu->rows[menu->selected[menu->page]] : NULL;
    return (struct ph_menu_result){ kind, row ? row->id : 0, row ? row->argument : 0, delta, menu->column };
}

static struct ph_menu_result ph_menu_nothing(void)
{
    return (struct ph_menu_result){ PH_MENU_NONE, 0, 0, 0, 0 };
}

struct ph_menu_result ph_menu_apply(struct ph_menu *menu, int nav)
{
    int *selected = &menu->selected[menu->page];
    int selectable = ph_menu_selectable(menu, *selected);
    int kind = menu->count ? menu->rows[*selected].kind : PH_ROW_GAP;

    switch (nav) {
    case PH_NAV_UP:
    case PH_NAV_DOWN:
        if (menu->count) {
            int step = nav == PH_NAV_UP ? -1 : 1;
            *selected = (*selected + step + menu->count) % menu->count;
            ph_menu_settle(menu, step);
            menu->follow = 1;
        }
        return ph_menu_nothing();
    case PH_NAV_LEFT:
    case PH_NAV_RIGHT:
        if (selectable && kind == PH_ROW_VALUE) {
            return ph_menu_result(menu, PH_MENU_ADJUST, nav == PH_NAV_LEFT ? -1 : 1);
        }
        if (selectable && kind == PH_ROW_PAIR) {
            menu->column = nav == PH_NAV_LEFT ? 0 : 1;
            return ph_menu_nothing();
        }
        return ph_menu_nothing();
    case PH_NAV_OK:
        return selectable ? ph_menu_result(menu, PH_MENU_ACTIVATE, 1) : ph_menu_nothing();
    case PH_NAV_BACK:
        return ph_menu_result(menu, PH_MENU_BACK, 0);
    case PH_NAV_CLEAR:
        return selectable && kind == PH_ROW_PAIR ? ph_menu_result(menu, PH_MENU_CLEAR, 0) : ph_menu_nothing();
    default:
        return ph_menu_nothing();
    }
}

static int ph_menu_stick(struct ph_menu *menu, const SDL_Event *event)
{
    int axis = event->gaxis.axis;
    if (axis != SDL_GAMEPAD_AXIS_LEFTX && axis != SDL_GAMEPAD_AXIS_LEFTY) {
        return PH_NAV_NONE;
    }
    int value = event->gaxis.value;
    int *latched = &menu->stick_latched[axis == SDL_GAMEPAD_AXIS_LEFTY];
    if (SDL_abs(value) < PH_STICK_RELEASE) {
        *latched = 0;
    }
    if (*latched || SDL_abs(value) < PH_STICK_PRESS) {
        return PH_NAV_NONE;
    }
    *latched = 1;
    if (axis == SDL_GAMEPAD_AXIS_LEFTY) {
        return value < 0 ? PH_NAV_UP : PH_NAV_DOWN;
    }
    return value < 0 ? PH_NAV_LEFT : PH_NAV_RIGHT;
}

int ph_menu_nav(struct ph_menu *menu, const SDL_Event *event)
{
    if (event->type == SDL_EVENT_KEY_DOWN) {
        if (event->key.mod & (SDL_KMOD_ALT | SDL_KMOD_GUI | SDL_KMOD_CTRL)) {
            return PH_NAV_NONE;
        }
        switch (event->key.scancode) {
        case SDL_SCANCODE_UP:
            return PH_NAV_UP;
        case SDL_SCANCODE_DOWN:
            return PH_NAV_DOWN;
        case SDL_SCANCODE_LEFT:
            return PH_NAV_LEFT;
        case SDL_SCANCODE_RIGHT:
            return PH_NAV_RIGHT;
        case SDL_SCANCODE_TAB:
            return event->key.repeat ? PH_NAV_NONE : PH_NAV_TOGGLE_PANE;
        case SDL_SCANCODE_PAGEUP:
            return PH_NAV_PREV_SECTION;
        case SDL_SCANCODE_PAGEDOWN:
            return PH_NAV_NEXT_SECTION;
        case SDL_SCANCODE_RETURN:
        case SDL_SCANCODE_KP_ENTER:
        case SDL_SCANCODE_SPACE:
            return event->key.repeat ? PH_NAV_NONE : PH_NAV_OK;
        case SDL_SCANCODE_ESCAPE:
        case SDL_SCANCODE_AC_BACK:
            return event->key.repeat ? PH_NAV_NONE : PH_NAV_BACK;
        case SDL_SCANCODE_DELETE:
        case SDL_SCANCODE_BACKSPACE:
            return PH_NAV_CLEAR;
        default:
            return PH_NAV_NONE;
        }
    }
    if (event->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
        switch (event->gbutton.button) {
        case SDL_GAMEPAD_BUTTON_DPAD_UP:
            return PH_NAV_UP;
        case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
            return PH_NAV_DOWN;
        case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
            return PH_NAV_LEFT;
        case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
            return PH_NAV_RIGHT;
        case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:
            return PH_NAV_PREV_SECTION;
        case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER:
            return PH_NAV_NEXT_SECTION;
        case SDL_GAMEPAD_BUTTON_SOUTH:
        case SDL_GAMEPAD_BUTTON_START:
            return PH_NAV_OK;
        case SDL_GAMEPAD_BUTTON_EAST:
        case SDL_GAMEPAD_BUTTON_BACK:
        case SDL_GAMEPAD_BUTTON_GUIDE:
            return PH_NAV_BACK;
        case SDL_GAMEPAD_BUTTON_NORTH:
        case SDL_GAMEPAD_BUTTON_WEST:
            return PH_NAV_CLEAR;
        default:
            return PH_NAV_NONE;
        }
    }
    if (event->type == SDL_EVENT_GAMEPAD_AXIS_MOTION) {
        return ph_menu_stick(menu, event);
    }
    return PH_NAV_NONE;
}

void ph_menu_edit(struct ph_menu *menu, SDL_Window *window, int id, const char *text, int numeric)
{
    menu->editing = 1;
    menu->edit_id = id;
    menu->edit_numeric = numeric;
    menu->edit_window = window;
    SDL_strlcpy(menu->edit_text, text ? text : "", sizeof menu->edit_text);
    if (window) {
        SDL_StartTextInput(window);
    }
}

void ph_menu_stop_editing(struct ph_menu *menu)
{
    if (menu->editing && menu->edit_window) {
        SDL_StopTextInput(menu->edit_window);
    }
    menu->editing = 0;
}

static struct ph_menu_result ph_menu_finish_edit(struct ph_menu *menu, int kind)
{
    int id = menu->edit_id;
    ph_menu_stop_editing(menu);
    return (struct ph_menu_result){ kind, id, 0, 0, 0 };
}

static int ph_menu_edit_allows(const struct ph_menu *menu, char letter)
{
    if (menu->edit_numeric) {
        return (letter >= '0' && letter <= '9') || letter == '.' || letter == ':';
    }
    return letter >= 32 && letter < 127;
}

static struct ph_menu_result ph_menu_edit_event(struct ph_menu *menu, const SDL_Event *event)
{
    if (event->type == SDL_EVENT_TEXT_INPUT) {
        size_t length = SDL_strlen(menu->edit_text);
        for (const char *letter = event->text.text; *letter && length + 1 < sizeof menu->edit_text; letter++) {
            if (ph_menu_edit_allows(menu, *letter)) {
                menu->edit_text[length++] = *letter;
                menu->edit_text[length] = 0;
            }
        }
        return ph_menu_nothing();
    }
    if (event->type == SDL_EVENT_KEY_DOWN) {
        SDL_Scancode key = event->key.scancode;
        size_t length = SDL_strlen(menu->edit_text);
        if (key == SDL_SCANCODE_BACKSPACE && length) {
            menu->edit_text[length - 1] = 0;
        } else if (key == SDL_SCANCODE_RETURN || key == SDL_SCANCODE_KP_ENTER) {
            return ph_menu_finish_edit(menu, PH_MENU_EDITED);
        } else if (key == SDL_SCANCODE_ESCAPE || key == SDL_SCANCODE_AC_BACK) {
            return ph_menu_finish_edit(menu, PH_MENU_EDIT_CANCELLED);
        }
        return ph_menu_nothing();
    }
    if (event->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
        if (event->gbutton.button == SDL_GAMEPAD_BUTTON_SOUTH || event->gbutton.button == SDL_GAMEPAD_BUTTON_START) {
            return ph_menu_finish_edit(menu, PH_MENU_EDITED);
        }
        if (event->gbutton.button == SDL_GAMEPAD_BUTTON_EAST || event->gbutton.button == SDL_GAMEPAD_BUTTON_BACK) {
            return ph_menu_finish_edit(menu, PH_MENU_EDIT_CANCELLED);
        }
        return ph_menu_nothing();
    }
    if (event->type == SDL_EVENT_FINGER_DOWN || (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN && event->button.which != SDL_TOUCH_MOUSEID)) {
        return ph_menu_finish_edit(menu, PH_MENU_EDITED);
    }
    return ph_menu_nothing();
}

static int ph_menu_pointer_ok(const SDL_Event *event)
{
    switch (event->type) {
    case SDL_EVENT_MOUSE_MOTION:
        return event->motion.which != SDL_TOUCH_MOUSEID;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
        return event->button.which != SDL_TOUCH_MOUSEID;
    case SDL_EVENT_MOUSE_WHEEL:
        return event->wheel.which != SDL_TOUCH_MOUSEID;
    case SDL_EVENT_FINGER_DOWN:
    case SDL_EVENT_FINGER_MOTION:
    case SDL_EVENT_FINGER_UP:
    case SDL_EVENT_FINGER_CANCELED:
        return event->tfinger.touchID != SDL_MOUSE_TOUCHID;
    default:
        return 0;
    }
}

static struct ph_menu_result ph_menu_release(struct ph_menu *menu, SDL_FPoint point)
{
    int row = menu->press_row;
    int zone = menu->press_zone;
    int secondary = menu->press_secondary;
    int back = menu->press_back;
    int parent = menu->press_parent;
    int dragged = menu->dragged;
    ph_menu_reset_pointer(menu);
    if (back && !dragged && ph_menu_inside(menu->back, point)) {
        return ph_menu_result(menu, PH_MENU_BACK, 0);
    }
    if (parent && !dragged && ph_menu_parent_at(menu, point)) {
        return ph_menu_result(menu, PH_MENU_BACK, 0);
    }
    if (dragged || row < 0 || ph_menu_row_at(menu, point) != row || !ph_menu_selectable(menu, row)) {
        return ph_menu_nothing();
    }
    menu->selected[menu->page] = row;
    int kind = menu->rows[row].kind;
    if (kind == PH_ROW_VALUE && (zone == PH_ZONE_LESS || secondary)) {
        return ph_menu_result(menu, PH_MENU_ADJUST, -1);
    }
    if (kind == PH_ROW_VALUE && zone == PH_ZONE_MORE) {
        return ph_menu_result(menu, PH_MENU_ADJUST, 1);
    }
    if (kind == PH_ROW_PAIR && (zone == PH_ZONE_FIRST || zone == PH_ZONE_SECOND)) {
        menu->column = zone == PH_ZONE_SECOND;
    }
    if (kind == PH_ROW_PAIR && secondary) {
        return ph_menu_result(menu, PH_MENU_CLEAR, 0);
    }
    return ph_menu_result(menu, PH_MENU_ACTIVATE, 1);
}

static struct ph_menu_result ph_menu_pointer(struct ph_menu *menu, SDL_Renderer *renderer, const SDL_Event *event)
{
    SDL_FPoint pixel;
    if (!ph_menu_pointer_ok(event) || !ph_view_event_point(renderer, event, &pixel)) {
        return ph_menu_nothing();
    }
    SDL_FPoint point = ph_menu_units(menu, pixel);
    int finger = event->type >= SDL_EVENT_FINGER_DOWN && event->type <= SDL_EVENT_FINGER_CANCELED;
    if (event->type == SDL_EVENT_MOUSE_WHEEL) {
        menu->scroll[menu->page] -= event->wheel.y * menu->row_height;
        ph_menu_clamp_scroll(menu);
        return ph_menu_nothing();
    }
    if (event->type == SDL_EVENT_FINGER_DOWN || event->type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        if (!finger && event->button.button == SDL_BUTTON_MIDDLE) {
            return ph_menu_nothing();
        }
        int back = ph_menu_inside(menu->back, point);
        int parent = !back && ph_menu_parent_at(menu, point);
        int row = (back || parent) ? -1 : ph_menu_row_at(menu, point);
        menu->pressing = 1;
        menu->press_finger = finger;
        menu->finger = finger ? event->tfinger.fingerID : 0;
        menu->press_row = row;
        menu->press_zone = row >= 0 ? ph_menu_zone(menu, row, point) : PH_ZONE_BODY;
        menu->press_secondary = !finger && event->button.button == SDL_BUTTON_RIGHT;
        menu->press_back = back;
        menu->press_parent = parent;
        menu->press_at = point;
        menu->press_scroll = menu->scroll[menu->page];
        menu->dragged = 0;
        if (!finger && ph_menu_selectable(menu, row)) {
            menu->selected[menu->page] = row;
        }
        return ph_menu_nothing();
    }
    int same = menu->pressing && menu->press_finger == finger && (!finger || event->tfinger.fingerID == menu->finger);
    if (event->type == SDL_EVENT_FINGER_MOTION || event->type == SDL_EVENT_MOUSE_MOTION) {
        if (same) {
            float moved = point.y - menu->press_at.y;
            if (SDL_fabsf(moved) * menu->unit > 8 * menu->points && ph_menu_scroll_limit(menu) > 0) {
                menu->dragged = 1;
            }
            if (menu->dragged) {
                menu->scroll[menu->page] = menu->press_scroll - moved;
                ph_menu_clamp_scroll(menu);
            }
        } else if (!finger && !menu->pressing) {
            int row = ph_menu_row_at(menu, point);
            if (ph_menu_selectable(menu, row)) {
                menu->selected[menu->page] = row;
            }
        }
        return ph_menu_nothing();
    }
    if (event->type == SDL_EVENT_FINGER_CANCELED) {
        ph_menu_reset_pointer(menu);
        return ph_menu_nothing();
    }
    return same ? ph_menu_release(menu, point) : ph_menu_nothing();
}

static void ph_menu_watch_input(struct ph_menu *menu, const SDL_Event *event)
{
    int touch = menu->touch;
    if (event->type == SDL_EVENT_FINGER_DOWN && event->tfinger.touchID != SDL_MOUSE_TOUCHID) {
        touch = 1;
    }
    if (event->type == SDL_EVENT_KEY_DOWN || event->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
        touch = 0;
    }
    if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN && event->button.which != SDL_TOUCH_MOUSEID) {
        touch = 0;
    }
    if (touch != menu->touch) {
        menu->touch = touch;
        menu->follow = 1;
    }
}

struct ph_menu_result ph_menu_event(struct ph_menu *menu, SDL_Renderer *renderer, const SDL_Event *event)
{
    if (menu->editing) {
        return ph_menu_edit_event(menu, event);
    }
    ph_menu_watch_input(menu, event);
    ph_menu_layout(menu, renderer);
    int nav = ph_menu_nav(menu, event);
    if (nav != PH_NAV_NONE) {
        return ph_menu_apply(menu, nav);
    }
    return ph_menu_pointer(menu, renderer, event);
}

void ph_menu_notice(struct ph_menu *menu, const char *text)
{
    SDL_strlcpy(menu->notice, text ? text : "", sizeof menu->notice);
    menu->notice_until = SDL_GetTicks() + PH_NOTICE_MILLISECONDS;
}

static SDL_FColor ph_menu_fade(SDL_FColor color, float alpha)
{
    return (SDL_FColor){ color.r, color.g, color.b, color.a * alpha };
}

static void ph_menu_write(const struct ph_menu *menu, SDL_Renderer *renderer, const char *text, float x, float y, float size, int weight, SDL_FColor color)
{
    ph_text(renderer, text, menu->origin.x + x * menu->unit, menu->origin.y + y * menu->unit, size * menu->unit, weight, color);
}

static void ph_menu_write_tracked(const struct ph_menu *menu, SDL_Renderer *renderer, const char *text, float x, float y, float size, float tracking, SDL_FColor color)
{
    char ch[2] = { 0, 0 };
    float pen = x;
    for (const char *p = text; *p; p++) {
        char c = (*p >= 'a' && *p <= 'z') ? (char)(*p - 32) : *p;
        ch[0] = c;
        ph_menu_write(menu, renderer, ch, pen, y, size, PH_TEXT_MEDIUM, color);
        pen += ph_menu_measure(menu, ch, size, PH_TEXT_MEDIUM) + (menu->unit >= 0.9f ? tracking : 0.0f);
    }
}

static float ph_menu_write_fit(const struct ph_menu *menu, SDL_Renderer *renderer, const char *text, float x, float y, float size, int weight, float room, SDL_FColor color)
{
    char fitted[PH_MENU_TEXT];
    ph_text_fit(text, size * menu->unit, weight, room * menu->unit, fitted, sizeof fitted);
    ph_menu_write(menu, renderer, fitted, x, y, size, weight, color);
    return ph_menu_measure(menu, fitted, size, weight);
}

static void ph_menu_write_right(const struct ph_menu *menu, SDL_Renderer *renderer, const char *text, float right, float y, float size, int weight, float room, SDL_FColor color)
{
    char fitted[PH_MENU_TEXT];
    ph_text_fit(text, size * menu->unit, weight, room * menu->unit, fitted, sizeof fitted);
    float width = ph_menu_measure(menu, fitted, size, weight);
    ph_menu_write(menu, renderer, fitted, right - width, y, size, weight, color);
}

static void ph_menu_write_center(const struct ph_menu *menu, SDL_Renderer *renderer, const char *text, float center, float y, float size, int weight, SDL_FColor color)
{
    float width = ph_menu_measure(menu, text, size, weight);
    ph_menu_write(menu, renderer, text, center - width * 0.5f, y, size, weight, color);
}

static void ph_menu_write_center_fit(const struct ph_menu *menu, SDL_Renderer *renderer, const char *text, float center, float y, float size, int weight, float room, SDL_FColor color)
{
    char fitted[PH_MENU_TEXT];
    ph_text_fit(text, size * menu->unit, weight, room * menu->unit, fitted, sizeof fitted);
    ph_menu_write_center(menu, renderer, fitted, center, y, size, weight, color);
}

static void ph_menu_write_wrapped(const struct ph_menu *menu, SDL_Renderer *renderer, const char *text, SDL_FRect box, float size, int weight, SDL_FColor color)
{
    float line_h = size * (menu->unit < 0.9f ? 1.2f : 1.32f);
    float y = box.y;
    while (*text) {
        int take = ph_menu_line(menu, text, size, weight, box.w);
        char line[PH_MENU_TEXT];
        SDL_strlcpy(line, text, (size_t)SDL_min(take + 1, (int)sizeof line));
        ph_menu_write(menu, renderer, line, box.x, y, size, weight, color);
        text += take;
        while (*text == ' ') {
            text++;
        }
        y += line_h;
    }
}

static void ph_menu_round(const struct ph_menu *menu, SDL_Renderer *renderer, SDL_FRect area, float radius, SDL_FColor color)
{
    ph_fill_round(renderer, ph_menu_pixels(menu, area), radius * menu->unit, color);
}

static void ph_menu_stroke_round_units(const struct ph_menu *menu, SDL_Renderer *renderer, SDL_FRect area, float radius, float thickness, SDL_FColor color)
{
    ph_stroke_round(renderer, ph_menu_pixels(menu, area), radius * menu->unit, SDL_max(1.0f, thickness * menu->unit), color);
}

static void ph_menu_draw_window_shadow(const struct ph_menu *menu, SDL_Renderer *renderer, SDL_FRect area, float radius, float offset, float alpha)
{
    struct ph_window_palette pal = ph_menu_palette(menu);
    SDL_FRect shifted = { area.x + offset, area.y + offset, area.w, area.h };
    SDL_FRect px = ph_menu_pixels(menu, shifted);
    float r = SDL_roundf(radius * menu->unit);
    ph_fill_round(renderer, px, r, ph_menu_fade(pal.outline, alpha));
}

static void ph_menu_draw_window(const struct ph_menu *menu, SDL_Renderer *renderer, SDL_FRect area, float radius)
{
    struct ph_window_palette pal = ph_menu_palette(menu);
    SDL_FRect px = ph_menu_pixels(menu, area);
    float u = menu->unit;
    float r = SDL_roundf(radius * u);
    float ow = SDL_max(1.0f, SDL_roundf(1.2f * u));
    float bw = SDL_max(1.0f, SDL_roundf(1.8f * u));

    SDL_FRect outer = { px.x - ow, px.y - ow, px.w + ow * 2.0f, px.h + ow * 2.0f };
    ph_fill_round(renderer, outer, r + ow, pal.outline);
    ph_fill_round(renderer, px, r, pal.border);

    SDL_FRect inner = { px.x + bw, px.y + bw, px.w - bw * 2.0f, px.h - bw * 2.0f };
    if (inner.w > 0 && inner.h > 0) {
        ph_fill_round(renderer, inner, SDL_max(r - bw, 1.5f), pal.interior);
    }
}

static void ph_menu_draw_tri_right(const struct ph_menu *menu, SDL_Renderer *renderer, float left_x, float cy, float w, float h, SDL_FColor color)
{
    float u = menu->unit;
    float px = menu->origin.x + left_x * u;
    float py = menu->origin.y + cy * u;
    SDL_FPoint a = { px, py - h * 0.5f * u };
    SDL_FPoint b = { px + w * u, py };
    SDL_FPoint c = { px, py + h * 0.5f * u };
    ph_fill_triangle(renderer, a, b, c, color);
}

static void ph_menu_draw_tri_left(const struct ph_menu *menu, SDL_Renderer *renderer, float right_x, float cy, float w, float h, SDL_FColor color)
{
    float u = menu->unit;
    float px = menu->origin.x + right_x * u;
    float py = menu->origin.y + cy * u;
    SDL_FPoint a = { px, py - h * 0.5f * u };
    SDL_FPoint b = { px - w * u, py };
    SDL_FPoint c = { px, py + h * 0.5f * u };
    ph_fill_triangle(renderer, a, b, c, color);
}

static int ph_menu_lit(const struct ph_menu *menu, int row)
{
    if (!ph_menu_selectable(menu, row)) {
        return 0;
    }
    if (menu->touch) {
        return menu->pressing && !menu->dragged && !menu->press_back && !menu->press_parent && menu->press_row == row;
    }
    return row == menu->selected[menu->page];
}

static const char *ph_menu_shown_value(const struct ph_menu *menu, const struct ph_row *row, char *buffer, size_t size, int is_active)
{
    if (is_active && menu->editing && row->id == menu->edit_id) {
        SDL_snprintf(buffer, size, "%s|", menu->edit_text);
        return buffer;
    }
    return row->value;
}

static void ph_menu_draw_value_control(const struct ph_menu *menu, SDL_Renderer *renderer, const struct ph_row *row, SDL_FRect box, const char *value, float size, int selected)
{
    struct ph_window_palette pal = ph_menu_palette(menu);
    SDL_FColor ink = row->disabled ? pal.dim : (selected ? pal.pill_ink : pal.text);
    SDL_FColor arrow_ink = row->disabled ? ph_menu_fade(pal.dim, 0.45f) : ink;
    float less_x = 0, more_x = 0;
    ph_menu_value_bounds(menu, box, &less_x, &more_x);
    float cy = box.y + box.h * 0.5f;
    float text_y = box.y + (box.h - size) * 0.5f;

    ph_menu_draw_tri_left(menu, renderer, less_x + 5.4f, cy, 5.4f, 7.0f, arrow_ink);
    ph_menu_draw_tri_right(menu, renderer, more_x - 5.4f, cy, 5.4f, 7.0f, arrow_ink);

    if (row->has_slider) {
        float val_w = ph_menu_measure(menu, value, size, PH_TEXT_MEDIUM);
        float val_right = more_x - 11.0f;
        ph_menu_write(menu, renderer, value, val_right - val_w, text_y, size, PH_TEXT_MEDIUM, ink);

        float bar_left = less_x + 13.0f;
        float bar_right = val_right - val_w - 7.5f;
        float bar_w = bar_right - bar_left;
        if (bar_w >= 24.0f) {
            float bar_h = 8.0f;
            SDL_FRect track = { bar_left, cy - bar_h * 0.5f, bar_w, bar_h };
            ph_menu_stroke_round_units(menu, renderer, track, 1.8f, 1.0f, ink);
            float frac = SDL_clamp(row->slider, 0.0f, 1.0f);
            if (frac > 0.01f) {
                float fill_w = SDL_max(3.0f, (bar_w - 1.6f) * frac);
                SDL_FRect fill = { bar_left + 0.8f, track.y + 0.8f, fill_w, bar_h - 1.6f };
                ph_menu_round(menu, renderer, fill, 1.2f, ink);
            }
        }
    } else {
        float center = (less_x + more_x) * 0.5f;
        float room = (more_x - less_x) - 16.0f;
        ph_menu_write_center_fit(menu, renderer, value, center, text_y, size, PH_TEXT_MEDIUM, room, ink);
    }
}

static void ph_menu_draw_card_row(const struct ph_menu *menu, SDL_Renderer *renderer, const struct ph_menu_layer *layer, int index, int is_active)
{
    const struct ph_menu_theme *theme = menu->theme ? menu->theme : &ph_menu_default_theme;
    struct ph_window_palette pal = ph_menu_palette(menu);
    const struct ph_row *row = &layer->rows[index];
    SDL_FRect box = layer->boxes[index];
    if (ph_menu_layer_scroll_limit(layer) > 0.0f) {
        box.y -= layer->scroll;
    }
    if (box.w <= 0 || box.h <= 0 || ph_menu_spacer(row)) {
        return;
    }
    float size = layer->sizes[index];
    if (row->kind == PH_ROW_HEADING) {
        float hy = box.y + (box.h - size) * 0.5f;
        float lx = (row->value[0] || row->other[0]) ? (box.x + 21.5f) : (box.x + 8.0f);
        ph_menu_write_tracked(menu, renderer, row->label, lx, hy, size, 1.0f, pal.dim);
        if (row->value[0] || row->other[0]) {
            float col0 = 0, col1 = 0;
            ph_menu_pair_columns(box, &col0, &col1);
            if (row->value[0]) {
                ph_menu_write_tracked(menu, renderer, row->value, col0, hy, size, 1.0f, pal.dim);
            }
            if (row->other[0]) {
                ph_menu_write_tracked(menu, renderer, row->other, col1, hy, size, 1.0f, pal.dim);
            }
        }
        return;
    }
    if (row->kind == PH_ROW_NOTE) {
        SDL_FColor note_ink = ph_menu_tone_color(theme, row->tone, pal.text);
        if (!row->value[0]) {
            ph_menu_write_wrapped(menu, renderer, row->label, box, size, ph_menu_note_weight(row), note_ink);
        } else {
            float y = box.y + (box.h - size) * 0.5f;
            ph_menu_write_fit(menu, renderer, row->label, box.x + 18.0f, y, size, PH_TEXT_MEDIUM, box.w * 0.55f, note_ink);
            ph_menu_write_right(menu, renderer, row->value, box.x + box.w - 10.0f, y, size, PH_TEXT_MEDIUM, box.w * 0.4f, pal.text);
        }
        return;
    }

    int selected = is_active ? ph_menu_lit(menu, index) : (ph_menu_row_selectable(layer->rows, layer->count, index) && index == layer->selected);
    SDL_FColor pill_col = is_active ? pal.pill_focus : pal.pill_active;
    SDL_FColor sel_ink = is_active ? pal.pill_ink : pal.interior;
    float cy = box.y + box.h * 0.5f;
    float text_y = box.y + (box.h - size) * 0.5f;
    float label_x = box.x + 21.5f;

    if (selected && row->kind == PH_ROW_PAIR) {
        float col0 = 0, col1 = 0;
        ph_menu_pair_columns(box, &col0, &col1);
        ph_menu_round(menu, renderer, box, PH_PILL_RADIUS, ph_menu_fade(pal.pill_active, 0.10f));
        float cell_x = layer->column ? col1 - 5.0f : col0 - 5.0f;
        float cell_w = layer->column ? (box.x + box.w - 4.0f - cell_x) : (col1 - col0 - 2.0f);
        ph_menu_round(menu, renderer, (SDL_FRect){ cell_x, box.y + 2.0f, cell_w, box.h - 4.0f }, PH_PILL_RADIUS - 1.0f, pill_col);
        ph_menu_draw_tri_right(menu, renderer, box.x + 8.5f, cy, 7.0f, 9.2f, pill_col);
    } else if (selected) {
        ph_menu_round(menu, renderer, box, PH_PILL_RADIUS, pill_col);
        ph_menu_draw_tri_right(menu, renderer, box.x + 8.5f, cy, 7.0f, 9.2f, sel_ink);
    }

    SDL_FColor label_ink = row->disabled ? pal.dim : (selected && row->kind != PH_ROW_PAIR ? sel_ink : pal.text);
    char buffer[PH_MENU_TEXT + 2];
    const char *value = ph_menu_shown_value(menu, row, buffer, sizeof buffer, is_active);

    if (row->kind == PH_ROW_VALUE) {
        float less_x = 0, more_x = 0;
        ph_menu_value_bounds(menu, box, &less_x, &more_x);
        ph_menu_write_fit(menu, renderer, row->label, label_x, text_y, size, PH_TEXT_MEDIUM, less_x - label_x - 8.0f, label_ink);
        ph_menu_draw_value_control(menu, renderer, row, box, value, size, selected && is_active);
    } else if (row->kind == PH_ROW_PAIR) {
        float col0 = 0, col1 = 0;
        ph_menu_pair_columns(box, &col0, &col1);
        ph_menu_write_fit(menu, renderer, row->label, label_x, text_y, size, PH_TEXT_MEDIUM, col0 - label_x - 8.0f, label_ink);
        for (int col = 0; col < 2; col++) {
            float cx = col ? col1 : col0;
            float cw = col ? (box.x + box.w - 8.0f - col1) : (col1 - col0 - 8.0f);
            int cell_lit = selected && layer->column == col;
            ph_menu_write_fit(menu, renderer, col ? row->other : value, cx, text_y, size,
                PH_TEXT_MEDIUM, cw, cell_lit ? sel_ink : pal.text);
        }
    } else {
        float room = value[0] ? (box.w - 30.0f) * 0.62f : (box.w - 26.0f);
        ph_menu_write_fit(menu, renderer, row->label, label_x, text_y, size, PH_TEXT_MEDIUM, room, label_ink);
        if (value[0]) {
            SDL_FColor v_ink = selected ? sel_ink : pal.dim;
            ph_menu_write_right(menu, renderer, value, box.x + box.w - 9.0f, text_y, size, PH_TEXT_MEDIUM, (box.w - 30.0f) * 0.36f, v_ink);
        }
    }
}

static void ph_menu_draw_layer_header(const struct ph_menu *menu, SDL_Renderer *renderer, const struct ph_menu_layer *layer, int level, int is_active)
{
    struct ph_window_palette pal = ph_menu_palette(menu);
    const char *caption = ph_menu_layer_caption(menu, layer, level);
    if (!caption[0] && layer->back.w <= 0) {
        return;
    }
    float hx = layer->title_at.x;
    float hy = layer->title_at.y;
    if (is_active && menu->press_back) {
        ph_menu_round(menu, renderer, layer->back, PH_PILL_RADIUS, ph_menu_fade(pal.pill_focus, 0.25f));
    }
    if (layer->back.w > 0) {
        ph_menu_draw_tri_left(menu, renderer, hx + 4.0f, hy + layer->title_size * 0.5f, 3.8f, 6.2f, pal.text);
        hx += 8.5f;
    }
    ph_menu_write_tracked(menu, renderer, caption, hx, hy, layer->title_size, 0.9f, pal.text);
}

static void ph_menu_draw_layer_scrollbar(const struct ph_menu *menu, SDL_Renderer *renderer, const struct ph_menu_layer *layer)
{
    float limit = ph_menu_layer_scroll_limit(layer);
    if (limit <= 0.0f) {
        return;
    }
    struct ph_window_palette pal = ph_menu_palette(menu);
    float length = SDL_max(14.0f, layer->list.h * layer->list.h / layer->content_height);
    float top = layer->list.y + (layer->list.h - length) * layer->scroll / limit;
    SDL_FRect bar = { layer->panel.x + layer->panel.w - 5.5f, top + 2.0f, 2.0f, length - 4.0f };
    ph_menu_round(menu, renderer, bar, 1.0f, ph_menu_fade(pal.border, 0.55f));
}

static void ph_menu_draw_layer(const struct ph_menu *menu, SDL_Renderer *renderer, const struct ph_menu_layer *layer, int level, int is_active)
{
    if (!layer->valid || layer->panel.w <= 0) {
        return;
    }
    if (level > 0) {
        ph_menu_draw_window_shadow(menu, renderer, layer->panel, PH_CARD_RADIUS, 4.5f, is_active ? 0.56f : 0.42f);
    } else {
        ph_menu_draw_window_shadow(menu, renderer, layer->panel, PH_CARD_RADIUS, 2.5f, 0.34f);
    }
    ph_menu_draw_window(menu, renderer, layer->panel, PH_CARD_RADIUS);
    ph_menu_draw_layer_header(menu, renderer, layer, level, is_active);

    int use_aside = (level == 0 && !menu->over_game && !layer->title[0] && menu->aside.w > 0.0f);
    int scrolls = ph_menu_layer_scroll_limit(layer) > 0.0f;
    if (scrolls) {
        SDL_FRect list = ph_menu_pixels(menu, layer->list);
        SDL_Rect clip = { (int)list.x, (int)list.y, (int)SDL_ceilf(list.w), (int)SDL_ceilf(list.h) };
        SDL_SetRenderClipRect(renderer, &clip);
    }
    for (int i = 0; i < layer->count; i++) {
        const struct ph_row *row = &layer->rows[i];
        if (use_aside && !ph_menu_item(row) && !ph_menu_spacer(row)) {
            continue;
        }
        SDL_FRect box = layer->boxes[i];
        if (scrolls) {
            box.y -= layer->scroll;
            if (box.y + box.h < layer->list.y || box.y > layer->list.y + layer->list.h) {
                continue;
            }
        }
        ph_menu_draw_card_row(menu, renderer, layer, i, is_active);
    }
    if (scrolls) {
        SDL_SetRenderClipRect(renderer, NULL);
        ph_menu_draw_layer_scrollbar(menu, renderer, layer);
    }

    if (!is_active) {
        struct ph_window_palette pal = ph_menu_palette(menu);
        SDL_FRect veil = { layer->panel.x + 2.0f, layer->panel.y + 2.0f, layer->panel.w - 4.0f, layer->panel.h - 4.0f };
        ph_menu_round(menu, renderer, veil, PH_CARD_RADIUS - 1.5f, ph_menu_fade(pal.interior, 0.38f));
    }
}

static SDL_FRect ph_menu_fit_picture(SDL_Texture *texture, SDL_FRect room, int cover)
{
    float width = 0;
    float height = 0;
    if (!SDL_GetTextureSize(texture, &width, &height) || width <= 0 || height <= 0) {
        return (SDL_FRect){ 0, 0, 0, 0 };
    }
    float scale = cover ? SDL_max(room.w / width, room.h / height) : SDL_min(room.w / width, room.h / height);
    return (SDL_FRect){ room.x + (room.w - width * scale) / 2, room.y + (room.h - height * scale) / 2, width * scale, height * scale };
}

static void ph_menu_draw_checker(struct ph_menu *menu, SDL_Renderer *renderer, SDL_FRect output, SDL_FColor a, SDL_FColor b)
{
    ph_fill_rect(renderer, output, a);
    float tile = SDL_max(16.0f, SDL_roundf(24.0f * menu->unit));
    int cols = (int)SDL_ceilf(output.w / tile) + 1;
    int rows = (int)SDL_ceilf(output.h / tile) + 1;
    for (int r = 0; r < rows; r++) {
        for (int c = (r & 1); c < cols; c += 2) {
            SDL_FRect cell = { output.x + c * tile, output.y + r * tile, tile, tile };
            ph_fill_rect(renderer, cell, b);
        }
    }
}

static void ph_menu_draw_background(struct ph_menu *menu, SDL_Renderer *renderer)
{
    struct ph_window_palette pal = ph_menu_palette(menu);
    SDL_FRect output = ph_view_output(renderer);
    if (menu->over_game) {
        float alpha = menu->depth > 1 ? 0.78f : 0.68f;
        ph_menu_draw_checker(menu, renderer, output, ph_menu_fade(pal.tile_a, alpha), ph_menu_fade(pal.tile_b, alpha + 0.08f));
        return;
    }
    if (menu->background) {
        ph_fill_rect(renderer, output, pal.tile_b);
        SDL_FRect target = ph_menu_fit_picture(menu->background, output, 1);
        SDL_RenderTexture(renderer, menu->background, NULL, &target);
        ph_fill_rect(renderer, output, ph_menu_fade(pal.tile_b, menu->depth > 1 ? 0.34f : 0.18f));
        return;
    }
    ph_menu_draw_checker(menu, renderer, output, pal.tile_a, pal.tile_b);
}

static void ph_menu_draw_logo(struct ph_menu *menu, SDL_Renderer *renderer)
{
    if (!menu->logo || menu->over_game) {
        return;
    }
    int low_res = menu->unit < 0.9f;
    int compact = menu->width <= 500.0f;
    float mx = low_res ? 12.0f : (compact ? 16.0f : 22.0f);
    float my = low_res ? 12.0f : (compact ? 16.0f : 22.0f);
    SDL_FRect root_panel = menu->layers[0].valid ? menu->layers[0].panel : menu->panel;
    float left = root_panel.x + root_panel.w + (low_res ? 12.0f : 20.0f);
    float avail_w = menu->width - left - mx;
    float max_h = menu->aside.h > 0.0f ? SDL_max(40.0f, menu->aside.y - my - 12.0f) : menu->height * 0.50f;
    float box_w = SDL_min(avail_w, compact ? 250.0f : 330.0f);
    float box_h = SDL_min(max_h, compact ? 136.0f : 168.0f);
    if (box_w <= 20.0f || box_h <= 20.0f) {
        return;
    }
    SDL_FRect room = { menu->width - mx - box_w, my, box_w, box_h };
    float tex_w = 0, tex_h = 0;
    if (!SDL_GetTextureSize(menu->logo, &tex_w, &tex_h) || tex_w <= 0 || tex_h <= 0) {
        return;
    }
    float scale = SDL_min(room.w / tex_w, room.h / tex_h);
    float draw_w = tex_w * scale;
    float draw_h = tex_h * scale;
    SDL_FRect placed = {
        SDL_max(left, menu->width - mx - draw_w),
        my + SDL_max(0.0f, (SDL_min(box_h, 128.0f) - draw_h) * 0.18f),
        draw_w,
        draw_h
    };
    SDL_FRect target = ph_menu_pixels(menu, placed);
    if (target.w > 0 && target.h > 0) {
        SDL_RenderTexture(renderer, menu->logo, NULL, &target);
    }
}

static void ph_menu_draw_aside(const struct ph_menu *menu, SDL_Renderer *renderer)
{
    if (menu->aside.w <= 0 || menu->aside.h <= 0 || !menu->layers[0].valid) {
        return;
    }
    const struct ph_menu_theme *theme = menu->theme ? menu->theme : &ph_menu_default_theme;
    struct ph_window_palette pal = ph_menu_palette(menu);
    ph_menu_draw_window_shadow(menu, renderer, menu->aside, PH_CARD_RADIUS, 2.5f, 0.34f);
    ph_menu_draw_window(menu, renderer, menu->aside, PH_CARD_RADIUS);
    const struct ph_menu_layer *root = &menu->layers[0];
    for (int i = 0; i < root->count; i++) {
        const struct ph_row *row = &root->rows[i];
        if (!ph_menu_item(row) && !ph_menu_spacer(row) && root->boxes[i].h > 0) {
            SDL_FColor color = ph_menu_tone_color(theme, row->tone, pal.text);
            ph_menu_write_wrapped(menu, renderer, row->label, root->boxes[i], root->sizes[i], ph_menu_note_weight(row), color);
        }
    }
    if (menu->depth > 1) {
        SDL_FRect veil = { menu->aside.x + 2.0f, menu->aside.y + 2.0f, menu->aside.w - 4.0f, menu->aside.h - 4.0f };
        ph_menu_round(menu, renderer, veil, PH_CARD_RADIUS - 1.5f, ph_menu_fade(pal.interior, 0.35f));
    }
}

static int ph_menu_noticing(const struct ph_menu *menu)
{
    return menu->notice[0] && SDL_GetTicks() < menu->notice_until;
}

static void ph_menu_draw_notice(struct ph_menu *menu, SDL_Renderer *renderer, float center)
{
    if (!ph_menu_noticing(menu)) {
        return;
    }
    struct ph_window_palette pal = ph_menu_palette(menu);
    char line[PH_MENU_NOTICE];
    float nsz = ph_menu_body_size(menu);
    float max_w = SDL_min(menu->width - 32.0f, 340.0f);
    ph_text_fit(menu->notice, nsz * menu->unit, PH_TEXT_MEDIUM, max_w * menu->unit, line, sizeof line);
    float text_w = ph_menu_measure(menu, line, nsz, PH_TEXT_MEDIUM);
    float width = SDL_max(96.0f, text_w + 24.0f);
    float h = nsz + 14.0f;
    float y = menu->height - h - 10.0f;
    float x = SDL_clamp(center - width * 0.5f, 12.0f, menu->width - width - 12.0f);
    SDL_FRect box = { x, y, width, h };
    ph_menu_draw_window(menu, renderer, box, 5.5f);
    ph_menu_write_center(menu, renderer, line, x + width * 0.5f, y + (h - nsz) * 0.5f, nsz, PH_TEXT_MEDIUM, pal.pill_focus);
}

void ph_menu_draw(struct ph_menu *menu, SDL_Renderer *renderer)
{
    ph_menu_layout(menu, renderer);
    struct ph_render_state state = ph_overlay_begin(renderer);
    ph_menu_draw_background(menu, renderer);
    ph_menu_draw_logo(menu, renderer);
    ph_menu_draw_aside(menu, renderer);

    int active = SDL_clamp(menu->depth - 1, 0, PH_MENU_DEPTH - 1);
    for (int lvl = 0; lvl < active; lvl++) {
        ph_menu_draw_layer(menu, renderer, &menu->layers[lvl], lvl, 0);
    }
    ph_menu_draw_layer(menu, renderer, &menu->layers[active], active, 1);

    ph_menu_draw_notice(menu, renderer, menu->width * 0.5f);
    ph_overlay_end(renderer, state);
}

void ph_menu_draw_notice_alone(struct ph_menu *menu, SDL_Renderer *renderer)
{
    if (!ph_menu_noticing(menu)) {
        return;
    }
    ph_menu_layout(menu, renderer);
    struct ph_render_state state = ph_overlay_begin(renderer);
    ph_menu_draw_notice(menu, renderer, menu->width * 0.5f);
    ph_overlay_end(renderer, state);
}

void ph_menu_prompt(struct ph_menu *menu, SDL_Renderer *renderer, const char *line, const char *strong, const char *hint)
{
    struct ph_window_palette pal = ph_menu_palette(menu);
    ph_menu_layout(menu, renderer);
    struct ph_render_state state = ph_overlay_begin(renderer);
    float w = SDL_min(menu->width - 48.0f, 280.0f);
    float h = 92.0f;
    float left = (menu->width - w) * 0.5f;
    float top = (menu->height - h) * 0.5f;
    float center = menu->width * 0.5f;
    ph_fill_rect(renderer, ph_view_output(renderer), ph_menu_fade(pal.outline, 0.65f));
    ph_menu_draw_window_shadow(menu, renderer, (SDL_FRect){ left, top, w, h }, PH_CARD_RADIUS, 4.5f, 0.56f);
    ph_menu_draw_window(menu, renderer, (SDL_FRect){ left, top, w, h }, PH_CARD_RADIUS);
    ph_menu_write_center(menu, renderer, line ? line : "", center, top + 16.0f, PH_BODY, PH_TEXT_MEDIUM, pal.text);
    ph_menu_write_center(menu, renderer, strong ? strong : "", center, top + 37.0f, 14.0f, PH_TEXT_BOLD, pal.pill_focus);
    ph_menu_write_center(menu, renderer, hint ? hint : "", center, top + 64.0f, PH_SMALL, PH_TEXT_MEDIUM, pal.dim);
    ph_overlay_end(renderer, state);
}

#endif
