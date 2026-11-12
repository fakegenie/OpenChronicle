#ifndef PH_DEBUG_H
#define PH_DEBUG_H

#include <SDL3/SDL.h>
#include "ph_draw.h"

#ifdef __cplusplus
extern "C" {
#endif

enum { PH_DEBUG_HISTORY = 240 };

struct ph_net;
struct ph_menu_theme;

struct ph_debug {
    const struct ph_menu_theme *theme;
    int visible;
    int tab;
    SDL_Keycode toggle_key;
    float frame_ms[PH_DEBUG_HISTORY];
    float work_ms[PH_DEBUG_HISTORY];
    int history_at;
    int history_count;
    uint64_t last_frame_ns;
    SDL_Renderer *renderer;
    struct ph_render_state saved;
    float scale;
    SDL_FRect panel;
    float content_top;
    float content_bottom;
    float content_height;
    float scroll;
    int tab_rows;
    int tab_index;
    float tab_x;
    float tab_y;
    float y;
    float last_right;
    float last_top;
    int same_line;
    SDL_FPoint pointer;
    int hovering;
    int pressing;
    SDL_FingerID finger;
    int dragged;
    SDL_FPoint press_at;
    float press_scroll;
    int clicked;
    SDL_FPoint click;
};

void ph_debug_init(struct ph_debug *debug);
void ph_debug_frame(struct ph_debug *debug, float work_ms);
int ph_debug_event(struct ph_debug *debug, SDL_Renderer *renderer, const SDL_Event *event);
int ph_debug_begin(struct ph_debug *debug, SDL_Renderer *renderer);
void ph_debug_end(struct ph_debug *debug);
int ph_debug_tab(struct ph_debug *debug, const char *name);
void ph_debug_same_line(struct ph_debug *debug);
void ph_debug_text(struct ph_debug *debug, const char *format, ...) SDL_PRINTF_VARARG_FUNC(2);
void ph_debug_heading(struct ph_debug *debug, const char *text);
int ph_debug_button(struct ph_debug *debug, const char *label);
int ph_debug_toggle(struct ph_debug *debug, const char *label, int *value);
int ph_debug_stepper(struct ph_debug *debug, const char *label, int *value, int minimum, int maximum, int step);
void ph_debug_plot(struct ph_debug *debug, const char *label, const float *values, int count, int start, float low, float high);
void ph_debug_memory(struct ph_debug *debug, const uint8_t *data, size_t size, uint32_t base, int *offset);
void ph_debug_performance(struct ph_debug *debug);
void ph_debug_network(struct ph_debug *debug, const struct ph_net *net);

#ifdef __cplusplus
}
#endif

#endif

#if defined(PH_IMPLEMENTATION) && !defined(PH_DEBUG_IMPLEMENTED)
#define PH_DEBUG_IMPLEMENTED

#include "ph_menu.h"
#include "ph_net.h"
#include "ph_view.h"
#include <stdarg.h>

enum {
    PH_DEBUG_GLYPH = 8,
    PH_DEBUG_LINE = 12,
    PH_DEBUG_TAB_LINE = 14,
    PH_DEBUG_PAD = 6,
    PH_DEBUG_MARGIN = 4,
    PH_DEBUG_WIDTH = 400,
    PH_DEBUG_PLOT = 40,
    PH_DEBUG_DRAG = 6,
    PH_DEBUG_MEMORY_ROWS = 16,
    PH_DEBUG_MEMORY_COLUMNS = 8,
};

static uint32_t ph_debug_hex(SDL_FColor c)
{
    uint32_t r = (uint32_t)(SDL_clamp(c.r, 0.0f, 1.0f) * 255.0f + 0.5f);
    uint32_t g = (uint32_t)(SDL_clamp(c.g, 0.0f, 1.0f) * 255.0f + 0.5f);
    uint32_t b = (uint32_t)(SDL_clamp(c.b, 0.0f, 1.0f) * 255.0f + 0.5f);
    return (r << 16) | (g << 8) | b;
}

static const struct ph_menu_theme *ph_debug_theme(const struct ph_debug *debug)
{
    return debug->theme ? debug->theme : &ph_menu_default_theme;
}

#define ph_debug_panel (ph_debug_hex(ph_debug_theme(debug)->panel))
#define ph_debug_border (ph_debug_hex(ph_debug_theme(debug)->border.a > 0.0f ? ph_debug_theme(debug)->border : ph_debug_theme(debug)->text))
#define ph_debug_casing (ph_debug_hex(ph_debug_theme(debug)->outline.a > 0.0f ? ph_debug_theme(debug)->outline : ph_debug_theme(debug)->panel))
#define ph_debug_ink (ph_debug_hex(ph_debug_theme(debug)->text))
#define ph_debug_dim (ph_debug_hex(ph_debug_theme(debug)->dim))
#define ph_debug_accent (ph_debug_hex(ph_debug_theme(debug)->accent))
#define ph_debug_good 0x7AE8B8u
#define ph_debug_bad (ph_debug_hex(ph_debug_theme(debug)->error))
#define ph_debug_box (ph_debug_hex(ph_debug_theme(debug)->tile_alt.a > 0.0f ? ph_debug_theme(debug)->tile_alt : ph_debug_theme(debug)->background))
#define ph_debug_box_hot (ph_debug_hex(ph_debug_theme(debug)->background))

void ph_debug_init(struct ph_debug *debug)
{
    SDL_zerop(debug);
    debug->toggle_key = SDLK_F1;
    debug->tab_rows = 1;
}

void ph_debug_frame(struct ph_debug *debug, float work_ms)
{
    uint64_t now = SDL_GetTicksNS();
    float frame_ms = debug->last_frame_ns ? (float)(now - debug->last_frame_ns) / 1000000.0f : 0;
    debug->last_frame_ns = now;
    debug->frame_ms[debug->history_at] = frame_ms;
    debug->work_ms[debug->history_at] = work_ms;
    debug->history_at = (debug->history_at + 1) % PH_DEBUG_HISTORY;
    debug->history_count = SDL_min(debug->history_count + 1, PH_DEBUG_HISTORY);
}

static float ph_debug_scale(SDL_Renderer *renderer)
{
    float points = ph_view_points(renderer) * (PH_MOBILE ? 1.5f : 1.0f);
    return SDL_max(1.0f, SDL_roundf(points));
}

static int ph_debug_inside(SDL_FRect rect, SDL_FPoint point)
{
    return point.x >= rect.x && point.y >= rect.y && point.x < rect.x + rect.w && point.y < rect.y + rect.h;
}

static int ph_debug_pointer_event(const SDL_Event *event)
{
    switch (event->type) {
    case SDL_EVENT_MOUSE_MOTION:
        return event->motion.which != SDL_TOUCH_MOUSEID;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
        return event->button.which != SDL_TOUCH_MOUSEID && event->button.button == SDL_BUTTON_LEFT;
    case SDL_EVENT_FINGER_DOWN:
    case SDL_EVENT_FINGER_MOTION:
    case SDL_EVENT_FINGER_UP:
    case SDL_EVENT_FINGER_CANCELED:
        return 1;
    default:
        return 0;
    }
}

static int ph_debug_is_finger(const SDL_Event *event)
{
    return event->type >= SDL_EVENT_FINGER_DOWN && event->type <= SDL_EVENT_FINGER_CANCELED;
}

static int ph_debug_toggle_event(struct ph_debug *debug, const SDL_Event *event)
{
    if (event->type == SDL_EVENT_KEY_DOWN && !event->key.repeat && event->key.key == debug->toggle_key) {
        return 1;
    }
    if (event->type == SDL_EVENT_FINGER_DOWN) {
        int count = 0;
        SDL_Finger **fingers = SDL_GetTouchFingers(event->tfinger.touchID, &count);
        SDL_free(fingers);
        return count >= 4;
    }
    return 0;
}

static int ph_debug_pointer(struct ph_debug *debug, const SDL_Event *event, SDL_FPoint point)
{
    int finger = ph_debug_is_finger(event);
    int down = event->type == SDL_EVENT_FINGER_DOWN || event->type == SDL_EVENT_MOUSE_BUTTON_DOWN;
    int up = event->type == SDL_EVENT_FINGER_UP || event->type == SDL_EVENT_FINGER_CANCELED || event->type == SDL_EVENT_MOUSE_BUTTON_UP;
    int ours = !finger || !debug->pressing || event->tfinger.fingerID == debug->finger;
    if (!finger) {
        debug->pointer = point;
        debug->hovering = 1;
    }
    if (down) {
        if (!ph_debug_inside(debug->panel, point) || debug->pressing) {
            return debug->pressing;
        }
        debug->pressing = 1;
        debug->finger = finger ? event->tfinger.fingerID : 0;
        debug->dragged = 0;
        debug->press_at = point;
        debug->press_scroll = debug->scroll;
        return 1;
    }
    if (!debug->pressing || !ours) {
        return ph_debug_inside(debug->panel, point) && !finger;
    }
    if (up) {
        debug->pressing = 0;
        if (!debug->dragged && event->type != SDL_EVENT_FINGER_CANCELED) {
            debug->clicked = 1;
            debug->click = point;
        }
        return 1;
    }
    float moved = point.y - debug->press_at.y;
    if (SDL_fabsf(moved) > PH_DEBUG_DRAG || SDL_fabsf(point.x - debug->press_at.x) > PH_DEBUG_DRAG) {
        debug->dragged = 1;
    }
    if (debug->dragged) {
        debug->scroll = debug->press_scroll - moved;
    }
    return 1;
}

int ph_debug_event(struct ph_debug *debug, SDL_Renderer *renderer, const SDL_Event *event)
{
    if (ph_debug_toggle_event(debug, event)) {
        debug->visible = !debug->visible;
        debug->pressing = 0;
        return 1;
    }
    if (!debug->visible) {
        return 0;
    }
    float scale = ph_debug_scale(renderer);
    if (event->type == SDL_EVENT_MOUSE_WHEEL && event->wheel.which != SDL_TOUCH_MOUSEID) {
        if (!ph_debug_inside(debug->panel, debug->pointer)) {
            return 0;
        }
        debug->scroll -= event->wheel.y * PH_DEBUG_LINE * 3;
        return 1;
    }
    SDL_FPoint point;
    if (!ph_debug_pointer_event(event) || !ph_view_event_point(renderer, event, &point)) {
        return 0;
    }
    point.x /= scale;
    point.y /= scale;
    return ph_debug_pointer(debug, event, point);
}

static void ph_debug_color(struct ph_debug *debug, uint32_t rgb, float alpha)
{
    SDL_FColor color = ph_color(rgb, alpha);
    SDL_SetRenderDrawColorFloat(debug->renderer, color.r, color.g, color.b, color.a);
}

static void ph_debug_fill(struct ph_debug *debug, SDL_FRect rect, uint32_t rgb, float alpha)
{
    ph_debug_color(debug, rgb, alpha);
    SDL_RenderFillRect(debug->renderer, &rect);
}

static void ph_debug_print(struct ph_debug *debug, float x, float y, const char *text, uint32_t rgb)
{
    ph_debug_color(debug, rgb, 1);
    SDL_RenderDebugText(debug->renderer, x, y, text);
}

static float ph_debug_text_width(const char *text)
{
    return (float)(SDL_utf8strlen(text) * PH_DEBUG_GLYPH);
}

static int ph_debug_clicked(struct ph_debug *debug, SDL_FRect rect)
{
    SDL_FRect grown = { rect.x - 2, rect.y - 2, rect.w + 4, rect.h + 4 };
    return debug->clicked && ph_debug_inside(grown, debug->click);
}

static int ph_debug_hot(struct ph_debug *debug, SDL_FRect rect)
{
    return debug->hovering && ph_debug_inside(rect, debug->pointer);
}

static int ph_debug_shown(const struct ph_debug *debug, SDL_FRect rect)
{
    return rect.y >= debug->content_top && rect.y + rect.h <= debug->content_bottom;
}

static SDL_FRect ph_debug_place(struct ph_debug *debug, float width, float height)
{
    float left = debug->panel.x + PH_DEBUG_PAD;
    float right = debug->panel.x + debug->panel.w - PH_DEBUG_PAD;
    SDL_FRect rect = { left, debug->y, width, height };
    if (debug->same_line && debug->last_right + PH_DEBUG_GLYPH + width <= right) {
        rect.x = debug->last_right + PH_DEBUG_GLYPH;
        rect.y = debug->last_top;
        debug->y = SDL_max(debug->y, rect.y + height + PH_DEBUG_LINE - PH_DEBUG_GLYPH);
    } else {
        debug->y += height + PH_DEBUG_LINE - PH_DEBUG_GLYPH;
    }
    debug->same_line = 0;
    debug->last_right = rect.x + width;
    debug->last_top = rect.y;
    return rect;
}

int ph_debug_begin(struct ph_debug *debug, SDL_Renderer *renderer)
{
    if (!debug->visible) {
        debug->clicked = 0;
        debug->hovering = 0;
        return 0;
    }
    debug->renderer = renderer;
    debug->saved = ph_overlay_begin(renderer);
    debug->scale = ph_debug_scale(renderer);
    SDL_SetRenderScale(renderer, debug->scale, debug->scale);
    SDL_FRect safe = ph_view_safe(renderer);
    float left = safe.x / debug->scale + PH_DEBUG_MARGIN;
    float top = safe.y / debug->scale + PH_DEBUG_MARGIN;
    float width = SDL_min((float)PH_DEBUG_WIDTH, safe.w / debug->scale - PH_DEBUG_MARGIN * 2);
    float room = safe.h / debug->scale - PH_DEBUG_MARGIN * 2;
    float tabs = (float)(debug->tab_rows * PH_DEBUG_TAB_LINE);
    float height = SDL_min(room, PH_DEBUG_PAD * 2 + tabs + PH_DEBUG_PAD + debug->content_height);
    debug->panel = (SDL_FRect){ left, top, width, height };
    debug->content_top = top + PH_DEBUG_PAD + tabs + PH_DEBUG_PAD;
    debug->content_bottom = top + height - PH_DEBUG_PAD;
    float most = SDL_max(0.0f, debug->content_height - (debug->content_bottom - debug->content_top));
    debug->scroll = SDL_clamp(debug->scroll, 0.0f, most);
    debug->y = debug->content_top - debug->scroll;
    debug->tab_x = left + PH_DEBUG_PAD;
    debug->tab_y = top + PH_DEBUG_PAD;
    debug->tab_rows = 1;
    debug->tab_index = 0;
    debug->same_line = 0;
    ph_debug_fill(debug, (SDL_FRect){ debug->panel.x - 1, debug->panel.y - 1, debug->panel.w + 2, debug->panel.h + 2 }, ph_debug_casing, 0.98f);
    ph_debug_fill(debug, debug->panel, ph_debug_border, 0.98f);
    ph_debug_fill(debug, (SDL_FRect){ debug->panel.x + 2, debug->panel.y + 2, debug->panel.w - 4, debug->panel.h - 4 }, ph_debug_panel, 0.97f);
    SDL_FRect close = { left + width - PH_DEBUG_PAD - PH_DEBUG_TAB_LINE, debug->tab_y, PH_DEBUG_TAB_LINE, PH_DEBUG_TAB_LINE - 2 };
    ph_debug_fill(debug, close, ph_debug_hot(debug, close) ? ph_debug_box_hot : ph_debug_box, 1);
    ph_debug_print(debug, close.x + 3, close.y + 3, "x", ph_debug_ink);
    if (ph_debug_clicked(debug, close)) {
        debug->visible = 0;
    }
    return 1;
}

void ph_debug_end(struct ph_debug *debug)
{
    if (!debug->renderer) {
        return;
    }
    debug->content_height = debug->y + debug->scroll - debug->content_top;
    float most = SDL_max(0.0f, debug->content_height - (debug->content_bottom - debug->content_top));
    if (most > 0) {
        float track = debug->content_bottom - debug->content_top;
        float thumb = SDL_max(8.0f, track * track / debug->content_height);
        float at = debug->content_top + (track - thumb) * debug->scroll / most;
        ph_debug_fill(debug, (SDL_FRect){ debug->panel.x + debug->panel.w - 3, at, 2, thumb }, ph_debug_dim, 0.8f);
    }
    ph_overlay_end(debug->renderer, debug->saved);
    debug->renderer = NULL;
    debug->clicked = 0;
}

int ph_debug_tab(struct ph_debug *debug, const char *name)
{
    int index = debug->tab_index++;
    float width = ph_debug_text_width(name) + PH_DEBUG_PAD * 2;
    float right = debug->panel.x + debug->panel.w - PH_DEBUG_PAD * 2 - PH_DEBUG_TAB_LINE;
    if (debug->tab_x + width > right && debug->tab_x > debug->panel.x + PH_DEBUG_PAD) {
        debug->tab_x = debug->panel.x + PH_DEBUG_PAD;
        debug->tab_y += PH_DEBUG_TAB_LINE;
        debug->tab_rows++;
    }
    SDL_FRect rect = { debug->tab_x, debug->tab_y, width, PH_DEBUG_TAB_LINE - 2 };
    debug->tab_x += width + 2;
    if (ph_debug_clicked(debug, rect)) {
        debug->tab = index;
        debug->scroll = 0;
    }
    int selected = debug->tab == index;
    ph_debug_fill(debug, rect, selected ? ph_debug_accent : ph_debug_hot(debug, rect) ? ph_debug_box_hot : ph_debug_box, selected ? 0.9f : 1);
    ph_debug_print(debug, rect.x + PH_DEBUG_PAD, rect.y + 3, name, selected ? ph_debug_panel : ph_debug_ink);
    return selected;
}

void ph_debug_same_line(struct ph_debug *debug)
{
    debug->same_line = 1;
}

static int ph_debug_break(const char *text, int room)
{
    int length = (int)SDL_strlen(text);
    if (length <= room) {
        return length;
    }
    int take = room;
    while (take > 0 && text[take] != ' ') {
        take--;
    }
    return take > 0 ? take : room;
}

static void ph_debug_line(struct ph_debug *debug, const char *text, uint32_t rgb)
{
    int room = SDL_max(8, (int)((debug->panel.w - PH_DEBUG_PAD * 2) / PH_DEBUG_GLYPH));
    const char *rest = text;
    do {
        char piece[256];
        int take = ph_debug_break(rest, SDL_min(room, (int)sizeof piece - 1));
        SDL_strlcpy(piece, rest, (size_t)take + 1);
        SDL_FRect rect = ph_debug_place(debug, ph_debug_text_width(piece), PH_DEBUG_GLYPH);
        if (ph_debug_shown(debug, rect)) {
            ph_debug_print(debug, rect.x, rect.y, piece, rgb);
        }
        rest += take;
        while (*rest == ' ') {
            rest++;
        }
    } while (*rest);
}

void ph_debug_text(struct ph_debug *debug, const char *format, ...)
{
    char text[256];
    va_list arguments;
    va_start(arguments, format);
    SDL_vsnprintf(text, sizeof text, format, arguments);
    va_end(arguments);
    ph_debug_line(debug, text, ph_debug_ink);
}

void ph_debug_heading(struct ph_debug *debug, const char *text)
{
    debug->y += PH_DEBUG_LINE / 3;
    ph_debug_line(debug, text, ph_debug_accent);
}

static int ph_debug_box_button(struct ph_debug *debug, SDL_FRect rect, const char *label)
{
    if (!ph_debug_shown(debug, rect)) {
        return 0;
    }
    ph_debug_fill(debug, rect, ph_debug_hot(debug, rect) ? ph_debug_box_hot : ph_debug_box, 1);
    ph_debug_print(debug, rect.x + 3, rect.y + 2, label, ph_debug_ink);
    return ph_debug_clicked(debug, rect);
}

int ph_debug_button(struct ph_debug *debug, const char *label)
{
    SDL_FRect rect = ph_debug_place(debug, ph_debug_text_width(label) + 6, PH_DEBUG_LINE);
    return ph_debug_box_button(debug, rect, label);
}

int ph_debug_toggle(struct ph_debug *debug, const char *label, int *value)
{
    SDL_FRect rect = ph_debug_place(debug, ph_debug_text_width(label) + PH_DEBUG_LINE + PH_DEBUG_GLYPH, PH_DEBUG_LINE);
    if (!ph_debug_shown(debug, rect)) {
        return 0;
    }
    SDL_FRect box = { rect.x, rect.y, PH_DEBUG_LINE, PH_DEBUG_LINE };
    ph_debug_fill(debug, box, ph_debug_hot(debug, rect) ? ph_debug_box_hot : ph_debug_box, 1);
    if (*value) {
        ph_debug_fill(debug, (SDL_FRect){ box.x + 3, box.y + 3, box.w - 6, box.h - 6 }, ph_debug_good, 1);
    }
    ph_debug_print(debug, rect.x + PH_DEBUG_LINE + PH_DEBUG_GLYPH, rect.y + 2, label, ph_debug_ink);
    if (!ph_debug_clicked(debug, rect)) {
        return 0;
    }
    *value = !*value;
    return 1;
}

int ph_debug_stepper(struct ph_debug *debug, const char *label, int *value, int minimum, int maximum, int step)
{
    char number[24];
    SDL_snprintf(number, sizeof number, "%d", *value);
    float label_width = SDL_max(ph_debug_text_width(label), 18.0f * PH_DEBUG_GLYPH);
    float number_width = SDL_max(ph_debug_text_width(number), 6.0f * PH_DEBUG_GLYPH);
    SDL_FRect rect = ph_debug_place(debug, label_width + number_width + PH_DEBUG_LINE * 2 + PH_DEBUG_GLYPH * 2, PH_DEBUG_LINE);
    if (!ph_debug_shown(debug, rect)) {
        return 0;
    }
    ph_debug_print(debug, rect.x, rect.y + 2, label, ph_debug_ink);
    SDL_FRect less = { rect.x + label_width, rect.y, PH_DEBUG_LINE, PH_DEBUG_LINE };
    SDL_FRect more = { less.x + PH_DEBUG_LINE + number_width + PH_DEBUG_GLYPH * 2, rect.y, PH_DEBUG_LINE, PH_DEBUG_LINE };
    ph_debug_print(debug, less.x + PH_DEBUG_LINE + PH_DEBUG_GLYPH, rect.y + 2, number, ph_debug_accent);
    int delta = ph_debug_box_button(debug, less, "<") ? -1 : 0;
    delta += ph_debug_box_button(debug, more, ">") ? 1 : 0;
    if (!delta) {
        return 0;
    }
    int changed = SDL_clamp(*value + delta * SDL_max(step, 1), minimum, maximum);
    if (changed == *value) {
        return 0;
    }
    *value = changed;
    return 1;
}

void ph_debug_plot(struct ph_debug *debug, const char *label, const float *values, int count, int start, float low, float high)
{
    float newest = count > 0 ? values[(start + count - 1) % count] : 0;
    float peak = low;
    for (int index = 0; index < count; index++) {
        peak = SDL_max(peak, values[index]);
    }
    ph_debug_text(debug, "%s %.2f  peak %.2f", label, (double)newest, (double)peak);
    float width = debug->panel.w - PH_DEBUG_PAD * 2;
    SDL_FRect rect = ph_debug_place(debug, width, PH_DEBUG_PLOT);
    if (!ph_debug_shown(debug, rect) || count < 2) {
        return;
    }
    ph_debug_fill(debug, rect, ph_debug_box, 1);
    float range = high > low ? high - low : 1;
    SDL_FPoint points[PH_DEBUG_HISTORY];
    int shown = SDL_min(count, PH_DEBUG_HISTORY);
    for (int index = 0; index < shown; index++) {
        float value = SDL_clamp(values[(start + count - shown + index) % count], low, high);
        points[index].x = rect.x + rect.w * (float)index / (float)(shown - 1);
        points[index].y = rect.y + rect.h - 1 - (rect.h - 2) * (value - low) / range;
    }
    ph_debug_color(debug, ph_debug_good, 1);
    SDL_RenderLines(debug->renderer, points, shown);
}

void ph_debug_memory(struct ph_debug *debug, const uint8_t *data, size_t size, uint32_t base, int *offset)
{
    int page = PH_DEBUG_MEMORY_ROWS * PH_DEBUG_MEMORY_COLUMNS;
    int last = size > 0 ? (int)((size - 1) / (size_t)page * (size_t)page) : 0;
    if (ph_debug_button(debug, "<<")) {
        *offset -= page * 16;
    }
    ph_debug_same_line(debug);
    if (ph_debug_button(debug, "<")) {
        *offset -= page;
    }
    ph_debug_same_line(debug);
    if (ph_debug_button(debug, ">")) {
        *offset += page;
    }
    ph_debug_same_line(debug);
    if (ph_debug_button(debug, ">>")) {
        *offset += page * 16;
    }
    *offset = SDL_clamp(*offset / page * page, 0, last);
    for (int row = 0; row < PH_DEBUG_MEMORY_ROWS; row++) {
        size_t at = (size_t)*offset + (size_t)(row * PH_DEBUG_MEMORY_COLUMNS);
        if (at >= size) {
            break;
        }
        char line[96];
        char letters[PH_DEBUG_MEMORY_COLUMNS + 1];
        int used = SDL_snprintf(line, sizeof line, "%06X ", (unsigned)(base + at));
        for (int column = 0; column < PH_DEBUG_MEMORY_COLUMNS; column++) {
            int inside = at + (size_t)column < size;
            uint8_t byte = inside ? data[at + (size_t)column] : 0;
            used += SDL_snprintf(line + used, sizeof line - (size_t)used, inside ? " %02X" : "   ", byte);
            letters[column] = inside && byte >= 32 && byte < 127 ? (char)byte : '.';
        }
        letters[PH_DEBUG_MEMORY_COLUMNS] = 0;
        SDL_snprintf(line + used, sizeof line - (size_t)used, "  %s", letters);
        ph_debug_line(debug, line, ph_debug_ink);
    }
}

void ph_debug_performance(struct ph_debug *debug)
{
    int count = debug->history_count;
    int start = (debug->history_at - count + PH_DEBUG_HISTORY) % PH_DEBUG_HISTORY;
    float frames = 0;
    float work = 0;
    for (int index = 0; index < count; index++) {
        frames += debug->frame_ms[(start + index) % PH_DEBUG_HISTORY];
        work += debug->work_ms[(start + index) % PH_DEBUG_HISTORY];
    }
    float average = count ? frames / count : 0;
    ph_debug_text(debug, "FPS %.1f  frame %.2f ms  work %.2f ms", (double)(average > 0 ? 1000 / average : 0), (double)average,
        (double)(count ? work / count : 0));
    ph_debug_plot(debug, "Frame ms", debug->frame_ms, count == PH_DEBUG_HISTORY ? PH_DEBUG_HISTORY : count,
        count == PH_DEBUG_HISTORY ? debug->history_at : 0, 0, 40);
    ph_debug_plot(debug, "Work ms", debug->work_ms, count == PH_DEBUG_HISTORY ? PH_DEBUG_HISTORY : count,
        count == PH_DEBUG_HISTORY ? debug->history_at : 0, 0, 20);
}

void ph_debug_network(struct ph_debug *debug, const struct ph_net *net)
{
    static const char *const statuses[] = { "Off", "Hosting", "Joining", "Connected", "Failed" };
    ph_debug_text(debug, "Status %s", statuses[SDL_clamp(net->status, 0, PH_NET_FAILED)]);
    ph_debug_text(debug, "Name %s  address %s", net->name, net->own_address[0] ? net->own_address : "-");
    if (net->status == PH_NET_CONNECTED) {
        ph_debug_text(debug, "Peer %s  player %d", net->peer_name, net->local_player + 1);
        ph_debug_text(debug, "Delay %d frames  frame %lld", net->delay, (long long)net->frame);
        ph_debug_text(debug, "Local through %lld  remote %lld", (long long)net->local_through, (long long)net->remote_through);
        ph_debug_text(debug, "Stalls %llu  last %.1f ms  total %.0f ms", (unsigned long long)net->stalls, (double)net->last_stall_us / 1000,
            (double)net->stall_us / 1000);
        ph_debug_text(debug, "Resends %llu", (unsigned long long)net->resends);
        ph_debug_text(debug, "Checks matched %llu  verified %lld", (unsigned long long)net->checks_matched, (long long)net->verified);
    }
    ph_debug_text(debug, "Packets sent %llu  received %llu", (unsigned long long)net->sent, (unsigned long long)net->received);
    if (net->loss || net->latency) {
        ph_debug_text(debug, "Simulating %d%% loss  %d ms latency  dropped %llu", net->loss, net->latency, (unsigned long long)net->dropped);
    }
    if (net->searching) {
        ph_debug_text(debug, "Games found %d", net->game_count);
    }
    if (net->message[0]) {
        ph_debug_line(debug, net->message, net->status == PH_NET_FAILED ? ph_debug_bad : ph_debug_dim);
    }
}

#endif
