#ifndef PH_TOUCH_H
#define PH_TOUCH_H

#include <SDL3/SDL.h>
#include <stdint.h>

#include "ph_input.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ph_setting;

enum {
    PH_TOUCH_SETTINGS = 5,
    PH_TOUCH_CONTROLS = 24,
    PH_TOUCH_FINGERS = 10,
    PH_TOUCH_NONE = -1,
};

enum {
    PH_TOUCH_PAD,
    PH_TOUCH_STICK,
    PH_TOUCH_BUTTON,
    PH_TOUCH_PILL,
    PH_TOUCH_PAUSE,
};

enum {
    PH_TOUCH_AUTO,
    PH_TOUCH_ALWAYS,
    PH_TOUCH_NEVER,
};

enum {
    PH_TOUCH_LANDSCAPE,
    PH_TOUCH_PORTRAIT,
};

enum {
    PH_TOUCH_UP = 1,
    PH_TOUCH_DOWN = 2,
    PH_TOUCH_LEFT = 4,
    PH_TOUCH_RIGHT = 8,
};

enum {
    PH_TOUCH_IGNORED,
    PH_TOUCH_CONSUMED,
    PH_TOUCH_EDITED,
    PH_TOUCH_EDIT_DONE,
};

struct ph_touch_control {
    const char *key;
    const char *label;
    int kind;
    float x;
    float y;
    float width;
    float height;
    int tap;
    uint32_t actions;
};

struct ph_touch_finger {
    int active;
    SDL_FingerID id;
    int kind;
    int control;
    SDL_FPoint origin;
    SDL_FPoint at;
    uint8_t directions;
};

struct ph_touch {
    const struct ph_touch_control *layouts[2];
    int count;
    int mode;
    int size;
    int opacity;
    int eight_way;
    int vibration;
    int seen;
    uint32_t hidden;
    uint32_t held;
    uint32_t tapped;
    uint8_t directions;
    SDL_FPoint places[2][PH_TOUCH_CONTROLS];
    uint32_t placed[2];
    struct ph_touch_finger fingers[PH_TOUCH_FINGERS];
    int editing;
    int grabbed;
    int edit_pressed;
    SDL_FPoint grab_offset;
    int orientation;
    float points;
    SDL_FRect safe;
    SDL_FRect rects[PH_TOUCH_CONTROLS];
    SDL_FRect done_rect;
    SDL_FRect reset_rect;
    SDL_Texture *layer;
    SDL_Haptic *haptic;
    int haptic_tried;
};

void ph_touch_init(struct ph_touch *touch, const struct ph_touch_control *landscape, const struct ph_touch_control *portrait, int count);
void ph_touch_close(struct ph_touch *touch);
int ph_touch_visible(const struct ph_touch *touch);
void ph_touch_layout(struct ph_touch *touch, SDL_Renderer *renderer);
int ph_touch_event(struct ph_touch *touch, SDL_Renderer *renderer, const SDL_Event *event);
int ph_touch_tapped(struct ph_touch *touch, int control);
int ph_touch_paused(struct ph_touch *touch);
uint32_t ph_touch_mask(const struct ph_touch *touch, const struct ph_input *input);
void ph_touch_release(struct ph_touch *touch);
void ph_touch_edit(struct ph_touch *touch, int on);
void ph_touch_reset_layout(struct ph_touch *touch);
int ph_touch_customized(const struct ph_touch *touch);
void ph_touch_draw(struct ph_touch *touch, SDL_Renderer *renderer);
int ph_touch_settings(struct ph_touch *touch, struct ph_setting *settings);
int ph_touch_take(struct ph_touch *touch, const char *key, const char *value);
void ph_touch_write(SDL_IOStream *file, struct ph_touch *touch);

extern const char *const ph_touch_modes[3];

#ifdef __cplusplus
}
#endif

#endif

#if defined(PH_IMPLEMENTATION) && !defined(PH_TOUCH_IMPLEMENTED)
#define PH_TOUCH_IMPLEMENTED

#include "ph_draw.h"
#include "ph_ios.h"
#include "ph_settings.h"
#include "ph_view.h"

enum {
    PH_TOUCH_EDIT_DONE_BUTTON = PH_TOUCH_CONTROLS,
    PH_TOUCH_EDIT_RESET_BUTTON,
};

static const SDL_FingerID ph_mouse_finger = (SDL_FingerID)-1;
static const char *const ph_orientation_keys[2] = { "touch_landscape_", "touch_portrait_" };
const char *const ph_touch_modes[3] = { "Auto", "Always", "Never" };

void ph_touch_init(struct ph_touch *touch, const struct ph_touch_control *landscape, const struct ph_touch_control *portrait, int count)
{
    SDL_zerop(touch);
    touch->layouts[PH_TOUCH_LANDSCAPE] = landscape;
    touch->layouts[PH_TOUCH_PORTRAIT] = portrait ? portrait : landscape;
    touch->count = SDL_clamp(count, 0, PH_TOUCH_CONTROLS);
    touch->mode = PH_TOUCH_AUTO;
    touch->size = 100;
    touch->opacity = 70;
    touch->vibration = PH_MOBILE;
    touch->seen = PH_MOBILE;
    touch->grabbed = PH_TOUCH_NONE;
    touch->edit_pressed = PH_TOUCH_NONE;
}

void ph_touch_close(struct ph_touch *touch)
{
    if (touch->layer) {
        SDL_DestroyTexture(touch->layer);
    }
    if (touch->haptic) {
        SDL_CloseHaptic(touch->haptic);
    }
    touch->layer = NULL;
    touch->haptic = NULL;
}

int ph_touch_visible(const struct ph_touch *touch)
{
    if (touch->editing) {
        return 1;
    }
    return touch->mode == PH_TOUCH_ALWAYS || (touch->mode == PH_TOUCH_AUTO && touch->seen);
}

static const struct ph_touch_control *ph_touch_controls(const struct ph_touch *touch)
{
    return touch->layouts[touch->orientation];
}

static SDL_FPoint ph_rect_center(SDL_FRect rect)
{
    return (SDL_FPoint){ rect.x + rect.w / 2, rect.y + rect.h / 2 };
}

static SDL_FRect ph_rect_around(SDL_FPoint center, float width, float height)
{
    return (SDL_FRect){ center.x - width / 2, center.y - height / 2, width, height };
}

static SDL_FRect ph_rect_grow(SDL_FRect rect, float by)
{
    return (SDL_FRect){ rect.x - by, rect.y - by, rect.w + by * 2, rect.h + by * 2 };
}

static int ph_rect_has(SDL_FRect rect, SDL_FPoint point)
{
    return point.x >= rect.x && point.x < rect.x + rect.w && point.y >= rect.y && point.y < rect.y + rect.h;
}

static SDL_FPoint ph_touch_clamp(const struct ph_touch *touch, SDL_FPoint center, float width, float height)
{
    SDL_FRect safe = touch->safe;
    return (SDL_FPoint){ SDL_clamp(center.x, safe.x + width / 2, safe.x + safe.w - width / 2), SDL_clamp(center.y, safe.y + height / 2, safe.y + safe.h - height / 2) };
}

void ph_touch_layout(struct ph_touch *touch, SDL_Renderer *renderer)
{
    touch->safe = ph_view_safe(renderer);
    touch->orientation = touch->safe.h > touch->safe.w ? PH_TOUCH_PORTRAIT : PH_TOUCH_LANDSCAPE;
    float points = ph_view_points(renderer);
    touch->points = points * (touch->size > 0 ? touch->size : 100) / 100.0f;
    const struct ph_touch_control *controls = ph_touch_controls(touch);
    SDL_FRect safe = touch->safe;
    for (int index = 0; index < touch->count; index++) {
        const struct ph_touch_control *control = &controls[index];
        float width = control->width * touch->points;
        float height = (control->height > 0 ? control->height : control->width) * touch->points;
        SDL_FPoint center = { safe.x + control->x * safe.w, safe.y + control->y * safe.h };
        if (touch->placed[touch->orientation] & 1u << index) {
            SDL_FPoint place = touch->places[touch->orientation][index];
            center = (SDL_FPoint){ safe.x + place.x * safe.w, safe.y + place.y * safe.h };
        }
        touch->rects[index] = ph_rect_around(ph_touch_clamp(touch, center, width, height), width, height);
    }
    float pill_width = 96 * points;
    float pill_height = 34 * points;
    float top = safe.y + 14 * points + pill_height / 2;
    float middle = safe.x + safe.w / 2;
    touch->reset_rect = ph_rect_around((SDL_FPoint){ middle - pill_width / 2 - 8 * points, top }, pill_width, pill_height);
    touch->done_rect = ph_rect_around((SDL_FPoint){ middle + pill_width / 2 + 8 * points, top }, pill_width, pill_height);
}

static int ph_touch_shown(const struct ph_touch *touch, int index)
{
    return touch->editing || !(touch->hidden & 1u << index);
}

static float ph_touch_radius(SDL_FRect rect)
{
    return SDL_max(rect.w, rect.h) / 2;
}

static float ph_touch_distance(SDL_FPoint from, SDL_FPoint to)
{
    return SDL_sqrtf((from.x - to.x) * (from.x - to.x) + (from.y - to.y) * (from.y - to.y));
}

static int ph_touch_nearest(const struct ph_touch *touch, SDL_FPoint point, int kind, float reach)
{
    const struct ph_touch_control *controls = ph_touch_controls(touch);
    int best = PH_TOUCH_NONE;
    float best_score = 0;
    for (int index = 0; index < touch->count; index++) {
        if (controls[index].kind != kind || !ph_touch_shown(touch, index)) {
            continue;
        }
        float radius = ph_touch_radius(touch->rects[index]);
        float score = ph_touch_distance(point, ph_rect_center(touch->rects[index])) / radius;
        if (score < reach && (best < 0 || score < best_score)) {
            best = index;
            best_score = score;
        }
    }
    return best;
}

static int ph_touch_on_control(const struct ph_touch *touch, SDL_FPoint point)
{
    const struct ph_touch_control *controls = ph_touch_controls(touch);
    for (int index = 0; index < touch->count; index++) {
        if (controls[index].kind != PH_TOUCH_STICK && ph_touch_shown(touch, index) && ph_rect_has(touch->rects[index], point)) {
            return 1;
        }
    }
    return 0;
}

static int ph_touch_pill(int kind)
{
    return kind == PH_TOUCH_PILL || kind == PH_TOUCH_PAUSE;
}

static int ph_touch_hit(const struct ph_touch *touch, SDL_FPoint point)
{
    const struct ph_touch_control *controls = ph_touch_controls(touch);
    float slack = 6 * touch->points;
    for (int index = 0; index < touch->count; index++) {
        if (ph_touch_pill(controls[index].kind) && ph_touch_shown(touch, index) && ph_rect_has(ph_rect_grow(touch->rects[index], slack), point)) {
            return index;
        }
    }
    int pad = ph_touch_nearest(touch, point, PH_TOUCH_PAD, 1.25f);
    int button = ph_touch_nearest(touch, point, PH_TOUCH_BUTTON, 1.4f);
    if (pad >= 0 && button >= 0) {
        float pad_score = ph_touch_distance(point, ph_rect_center(touch->rects[pad])) / ph_touch_radius(touch->rects[pad]);
        float button_score = ph_touch_distance(point, ph_rect_center(touch->rects[button])) / ph_touch_radius(touch->rects[button]);
        return pad_score < button_score ? pad : button;
    }
    if (pad >= 0 || button >= 0) {
        return pad >= 0 ? pad : button;
    }
    SDL_FRect safe = touch->safe;
    float middle = safe.x + safe.w / 2;
    for (int index = 0; index < touch->count; index++) {
        if (controls[index].kind != PH_TOUCH_STICK || !ph_touch_shown(touch, index)) {
            continue;
        }
        int left = ph_rect_center(touch->rects[index]).x < middle;
        int same_side = left ? point.x < middle : point.x >= middle;
        if (same_side && !ph_touch_on_control(touch, point)) {
            return index;
        }
    }
    return PH_TOUCH_NONE;
}

static uint8_t ph_touch_way(float dx, float dy, uint8_t before, int eight_way)
{
    if (eight_way) {
        static const uint8_t sectors[8] = {
            PH_TOUCH_RIGHT, PH_TOUCH_RIGHT | PH_TOUCH_DOWN, PH_TOUCH_DOWN, PH_TOUCH_DOWN | PH_TOUCH_LEFT,
            PH_TOUCH_LEFT, PH_TOUCH_LEFT | PH_TOUCH_UP, PH_TOUCH_UP, PH_TOUCH_UP | PH_TOUCH_RIGHT,
        };
        int sector = ((int)SDL_lroundf(SDL_atan2f(dy, dx) / (SDL_PI_F / 4)) + 8) % 8;
        return sectors[sector];
    }
    int sideways = SDL_fabsf(dx) > SDL_fabsf(dy);
    if ((before & (PH_TOUCH_LEFT | PH_TOUCH_RIGHT)) && SDL_fabsf(dx) * 1.2f >= SDL_fabsf(dy)) {
        sideways = 1;
    } else if ((before & (PH_TOUCH_UP | PH_TOUCH_DOWN)) && SDL_fabsf(dy) * 1.2f >= SDL_fabsf(dx)) {
        sideways = 0;
    }
    if (sideways) {
        return dx < 0 ? PH_TOUCH_LEFT : PH_TOUCH_RIGHT;
    }
    return dy < 0 ? PH_TOUCH_UP : PH_TOUCH_DOWN;
}

static uint8_t ph_touch_aim(const struct ph_touch *touch, const struct ph_touch_finger *finger)
{
    float radius = ph_touch_radius(touch->rects[finger->control]);
    float dx = finger->at.x - finger->origin.x;
    float dy = finger->at.y - finger->origin.y;
    float dead = radius * (finger->kind == PH_TOUCH_STICK ? 0.16f : 0.18f);
    if (dx * dx + dy * dy < dead * dead) {
        return 0;
    }
    return ph_touch_way(dx, dy, finger->directions, touch->eight_way || finger->kind == PH_TOUCH_STICK);
}

static void ph_touch_buzz(struct ph_touch *touch)
{
    if (!touch->vibration) {
        return;
    }
#if defined(SDL_PLATFORM_IOS)
    ph_ios_tap();
#else
    if (!touch->haptic_tried) {
        touch->haptic_tried = 1;
        int count = 0;
        SDL_HapticID *ids = SDL_InitSubSystem(SDL_INIT_HAPTIC) ? SDL_GetHaptics(&count) : NULL;
        touch->haptic = ids && count > 0 ? SDL_OpenHaptic(ids[0]) : NULL;
        SDL_free(ids);
        if (touch->haptic && !SDL_InitHapticRumble(touch->haptic)) {
            SDL_CloseHaptic(touch->haptic);
            touch->haptic = NULL;
        }
    }
    if (touch->haptic) {
        SDL_PlayHapticRumble(touch->haptic, 0.3f, 12);
    }
#endif
}

static void ph_touch_settle(struct ph_touch *touch)
{
    const struct ph_touch_control *controls = ph_touch_controls(touch);
    uint32_t before = touch->held;
    float slack = 6 * touch->points;
    touch->held = 0;
    touch->directions = 0;
    for (int slot = 0; slot < PH_TOUCH_FINGERS; slot++) {
        struct ph_touch_finger *finger = &touch->fingers[slot];
        if (!finger->active || finger->control < 0) {
            continue;
        }
        if (ph_touch_pill(finger->kind) && !ph_rect_has(ph_rect_grow(touch->rects[finger->control], slack), finger->at)) {
            continue;
        }
        touch->held |= 1u << finger->control;
        touch->directions |= finger->directions;
    }
    uint32_t pressed = touch->held & ~before;
    for (int index = 0; index < touch->count; index++) {
        if (pressed & 1u << index && controls[index].kind != PH_TOUCH_PAD && controls[index].kind != PH_TOUCH_STICK) {
            ph_touch_buzz(touch);
            return;
        }
    }
}

void ph_touch_release(struct ph_touch *touch)
{
    SDL_zeroa(touch->fingers);
    touch->held = 0;
    touch->directions = 0;
}

static int ph_touch_finger_slot(const struct ph_touch *touch, SDL_FingerID id)
{
    for (int slot = 0; slot < PH_TOUCH_FINGERS; slot++) {
        if (touch->fingers[slot].active && touch->fingers[slot].id == id) {
            return slot;
        }
    }
    return -1;
}

static int ph_touch_free_slot(const struct ph_touch *touch)
{
    for (int slot = 0; slot < PH_TOUCH_FINGERS; slot++) {
        if (!touch->fingers[slot].active) {
            return slot;
        }
    }
    return -1;
}

static int ph_touch_taken(const struct ph_touch *touch, int control)
{
    for (int slot = 0; slot < PH_TOUCH_FINGERS; slot++) {
        if (touch->fingers[slot].active && touch->fingers[slot].control == control) {
            return 1;
        }
    }
    return 0;
}

static int ph_touch_down(struct ph_touch *touch, SDL_FingerID id, SDL_FPoint point)
{
    const struct ph_touch_control *controls = ph_touch_controls(touch);
    int slot = ph_touch_free_slot(touch);
    int control = ph_touch_hit(touch, point);
    if (slot < 0 || control < 0) {
        return PH_TOUCH_IGNORED;
    }
    int kind = controls[control].kind;
    if ((kind == PH_TOUCH_PAD || kind == PH_TOUCH_STICK) && ph_touch_taken(touch, control)) {
        return PH_TOUCH_CONSUMED;
    }
    struct ph_touch_finger *finger = &touch->fingers[slot];
    *finger = (struct ph_touch_finger){ 1, id, kind, control, point, point, 0 };
    if (kind == PH_TOUCH_PAD) {
        finger->origin = ph_rect_center(touch->rects[control]);
    }
    if (kind == PH_TOUCH_PAD || kind == PH_TOUCH_STICK) {
        finger->directions = ph_touch_aim(touch, finger);
    }
    ph_touch_settle(touch);
    return PH_TOUCH_CONSUMED;
}

static int ph_touch_move(struct ph_touch *touch, SDL_FingerID id, SDL_FPoint point)
{
    int slot = ph_touch_finger_slot(touch, id);
    if (slot < 0) {
        return PH_TOUCH_IGNORED;
    }
    struct ph_touch_finger *finger = &touch->fingers[slot];
    finger->at = point;
    if (finger->kind == PH_TOUCH_PAD || finger->kind == PH_TOUCH_STICK) {
        finger->directions = ph_touch_aim(touch, finger);
    } else if (finger->kind == PH_TOUCH_BUTTON) {
        finger->control = ph_touch_nearest(touch, point, PH_TOUCH_BUTTON, 1.6f);
    }
    ph_touch_settle(touch);
    return PH_TOUCH_CONSUMED;
}

static int ph_touch_up(struct ph_touch *touch, SDL_FingerID id, SDL_FPoint point, int cancelled)
{
    int slot = ph_touch_finger_slot(touch, id);
    if (slot < 0) {
        return PH_TOUCH_IGNORED;
    }
    struct ph_touch_finger *finger = &touch->fingers[slot];
    const struct ph_touch_control *controls = ph_touch_controls(touch);
    int taps = finger->control >= 0 && (controls[finger->control].tap || controls[finger->control].kind == PH_TOUCH_PAUSE);
    if (!cancelled && taps &&
        ph_rect_has(ph_rect_grow(touch->rects[finger->control], 6 * touch->points), point)) {
        touch->tapped |= 1u << finger->control;
    }
    finger->active = 0;
    ph_touch_settle(touch);
    return PH_TOUCH_CONSUMED;
}

static void ph_touch_move_control(struct ph_touch *touch, int control, SDL_FPoint center)
{
    SDL_FRect rect = touch->rects[control];
    SDL_FRect safe = touch->safe;
    SDL_FPoint placed = ph_touch_clamp(touch, center, rect.w, rect.h);
    touch->rects[control] = ph_rect_around(placed, rect.w, rect.h);
    touch->places[touch->orientation][control] = (SDL_FPoint){ (placed.x - safe.x) / safe.w, (placed.y - safe.y) / safe.h };
    touch->placed[touch->orientation] |= 1u << control;
}

static int ph_touch_edit_target(const struct ph_touch *touch, SDL_FPoint point)
{
    float slack = 6 * touch->points;
    if (ph_rect_has(ph_rect_grow(touch->done_rect, slack), point)) {
        return PH_TOUCH_EDIT_DONE_BUTTON;
    }
    if (ph_rect_has(ph_rect_grow(touch->reset_rect, slack), point)) {
        return PH_TOUCH_EDIT_RESET_BUTTON;
    }
    for (int index = touch->count - 1; index >= 0; index--) {
        if (ph_rect_has(ph_rect_grow(touch->rects[index], slack), point)) {
            return index;
        }
    }
    return PH_TOUCH_NONE;
}

static int ph_touch_pointer_kind(const SDL_Event *event)
{
    switch (event->type) {
    case SDL_EVENT_FINGER_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        return 1;
    case SDL_EVENT_FINGER_MOTION:
    case SDL_EVENT_MOUSE_MOTION:
        return 2;
    case SDL_EVENT_FINGER_UP:
    case SDL_EVENT_MOUSE_BUTTON_UP:
        return 3;
    case SDL_EVENT_FINGER_CANCELED:
        return 4;
    default:
        return 0;
    }
}

static int ph_touch_mouse(const SDL_Event *event)
{
    if (event->type == SDL_EVENT_MOUSE_MOTION) {
        return event->motion.which != SDL_TOUCH_MOUSEID;
    }
    if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN || event->type == SDL_EVENT_MOUSE_BUTTON_UP) {
        return event->button.which != SDL_TOUCH_MOUSEID && event->button.button == SDL_BUTTON_LEFT;
    }
    return 0;
}

static int ph_touch_finger(const SDL_Event *event)
{
    return event->type >= SDL_EVENT_FINGER_DOWN && event->type <= SDL_EVENT_FINGER_CANCELED && event->tfinger.touchID != SDL_MOUSE_TOUCHID;
}

static int ph_touch_edit_event(struct ph_touch *touch, int phase, SDL_FPoint point)
{
    if (phase == 1) {
        int target = ph_touch_edit_target(touch, point);
        touch->grabbed = target >= 0 && target < touch->count ? target : PH_TOUCH_NONE;
        touch->edit_pressed = target >= PH_TOUCH_EDIT_DONE_BUTTON ? target : PH_TOUCH_NONE;
        if (touch->grabbed >= 0) {
            SDL_FPoint center = ph_rect_center(touch->rects[touch->grabbed]);
            touch->grab_offset = (SDL_FPoint){ center.x - point.x, center.y - point.y };
        }
        return PH_TOUCH_CONSUMED;
    }
    if (phase == 2) {
        if (touch->grabbed >= 0) {
            ph_touch_move_control(touch, touch->grabbed, (SDL_FPoint){ point.x + touch->grab_offset.x, point.y + touch->grab_offset.y });
        }
        return PH_TOUCH_CONSUMED;
    }
    int grabbed = touch->grabbed;
    int pressed = touch->edit_pressed;
    touch->grabbed = PH_TOUCH_NONE;
    touch->edit_pressed = PH_TOUCH_NONE;
    if (grabbed >= 0) {
        return PH_TOUCH_EDITED;
    }
    if (phase == 4 || pressed < 0 || ph_touch_edit_target(touch, point) != pressed) {
        return PH_TOUCH_CONSUMED;
    }
    if (pressed == PH_TOUCH_EDIT_RESET_BUTTON) {
        ph_touch_reset_layout(touch);
        return PH_TOUCH_EDITED;
    }
    ph_touch_edit(touch, 0);
    return PH_TOUCH_EDIT_DONE;
}

static int ph_touch_leave_key(const SDL_Event *event)
{
    if (event->type == SDL_EVENT_KEY_DOWN) {
        return event->key.scancode == SDL_SCANCODE_ESCAPE || event->key.scancode == SDL_SCANCODE_AC_BACK || event->key.scancode == SDL_SCANCODE_RETURN;
    }
    return event->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN && (event->gbutton.button == SDL_GAMEPAD_BUTTON_EAST || event->gbutton.button == SDL_GAMEPAD_BUTTON_START);
}

int ph_touch_event(struct ph_touch *touch, SDL_Renderer *renderer, const SDL_Event *event)
{
    int finger = ph_touch_finger(event);
    int mouse = ph_touch_mouse(event);
    if (event->type == SDL_EVENT_FINGER_DOWN && finger) {
        touch->seen = 1;
    }
    if ((event->type == SDL_EVENT_KEY_DOWN && event->key.scancode != SDL_SCANCODE_AC_BACK) || event->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN ||
        event->type == SDL_EVENT_GAMEPAD_ADDED || (event->type == SDL_EVENT_MOUSE_MOTION && mouse && !PH_MOBILE && !touch->editing)) {
        touch->seen = 0;
    }
    if (event->type == SDL_EVENT_WINDOW_FOCUS_LOST || event->type == SDL_EVENT_WILL_ENTER_BACKGROUND || event->type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
        ph_touch_release(touch);
        return PH_TOUCH_IGNORED;
    }
    if (touch->editing && ph_touch_leave_key(event)) {
        ph_touch_edit(touch, 0);
        return PH_TOUCH_EDIT_DONE;
    }
    int phase = ph_touch_pointer_kind(event);
    SDL_FPoint point;
    if (!phase || (!finger && !mouse) || !ph_view_event_point(renderer, event, &point)) {
        return PH_TOUCH_IGNORED;
    }
    ph_touch_layout(touch, renderer);
    if (touch->editing) {
        return ph_touch_edit_event(touch, phase, point);
    }
    if (!ph_touch_visible(touch)) {
        ph_touch_release(touch);
        return PH_TOUCH_IGNORED;
    }
    SDL_FingerID id = finger ? event->tfinger.fingerID : ph_mouse_finger;
    if (phase == 1) {
        return ph_touch_down(touch, id, point);
    }
    if (phase == 2) {
        return ph_touch_move(touch, id, point);
    }
    return ph_touch_up(touch, id, point, phase == 4);
}

int ph_touch_tapped(struct ph_touch *touch, int control)
{
    int tapped = (touch->tapped & 1u << control) != 0;
    touch->tapped &= ~(1u << control);
    return tapped;
}

static uint32_t ph_touch_direction_actions(const struct ph_input *input, int button)
{
    uint32_t mask = 0;
    for (int action = 0; action < input->actions; action++) {
        struct ph_binding binding = input->defaults[0][action][PH_SLOT_PAD];
        mask |= binding.kind == PH_BIND_BUTTON && binding.code == button ? 1u << action : 0;
    }
    return mask;
}

uint32_t ph_touch_mask(const struct ph_touch *touch, const struct ph_input *input)
{
    const struct ph_touch_control *controls = ph_touch_controls(touch);
    uint32_t mask = 0;
    mask |= touch->directions & PH_TOUCH_UP ? ph_touch_direction_actions(input, SDL_GAMEPAD_BUTTON_DPAD_UP) : 0;
    mask |= touch->directions & PH_TOUCH_DOWN ? ph_touch_direction_actions(input, SDL_GAMEPAD_BUTTON_DPAD_DOWN) : 0;
    mask |= touch->directions & PH_TOUCH_LEFT ? ph_touch_direction_actions(input, SDL_GAMEPAD_BUTTON_DPAD_LEFT) : 0;
    mask |= touch->directions & PH_TOUCH_RIGHT ? ph_touch_direction_actions(input, SDL_GAMEPAD_BUTTON_DPAD_RIGHT) : 0;
    for (int index = 0; index < touch->count; index++) {
        mask |= touch->held & 1u << index ? controls[index].actions : 0;
    }
    return mask;
}

int ph_touch_paused(struct ph_touch *touch)
{
    const struct ph_touch_control *controls = ph_touch_controls(touch);
    int paused = 0;
    for (int index = 0; index < touch->count; index++) {
        if (controls[index].kind == PH_TOUCH_PAUSE && ph_touch_tapped(touch, index)) {
            paused = 1;
        }
    }
    return paused;
}

void ph_touch_edit(struct ph_touch *touch, int on)
{
    touch->editing = on;
    touch->grabbed = PH_TOUCH_NONE;
    touch->edit_pressed = PH_TOUCH_NONE;
    ph_touch_release(touch);
}

void ph_touch_reset_layout(struct ph_touch *touch)
{
    touch->placed[PH_TOUCH_LANDSCAPE] = 0;
    touch->placed[PH_TOUCH_PORTRAIT] = 0;
}

int ph_touch_customized(const struct ph_touch *touch)
{
    return touch->placed[PH_TOUCH_LANDSCAPE] || touch->placed[PH_TOUCH_PORTRAIT];
}

static SDL_FColor ph_white(float alpha)
{
    return (SDL_FColor){ 1, 1, 1, alpha };
}

static float ph_touch_line(const struct ph_touch *touch)
{
    return SDL_max(1.5f, 1.6f * touch->points);
}

static void ph_touch_draw_label(SDL_Renderer *renderer, const char *text, SDL_FRect rect, float size, int pressed)
{
    char fitted[64];
    ph_text_fit(text, size, PH_TEXT_BOLD, rect.w * 0.82f, fitted, sizeof fitted);
    ph_text_centered(renderer, fitted, rect.x + rect.w / 2, rect.y + rect.h / 2, size, PH_TEXT_BOLD, ph_white(pressed ? 1 : 0.85f));
}

static void ph_touch_draw_round(SDL_Renderer *renderer, const struct ph_touch *touch, SDL_FRect rect, const char *label, int pressed)
{
    float radius = SDL_min(rect.w, rect.h) / 2;
    ph_fill_round(renderer, rect, radius, ph_white(pressed ? 0.45f : 0.12f));
    ph_stroke_round(renderer, rect, radius, ph_touch_line(touch), ph_white(pressed ? 0.95f : 0.6f));
    if (label && *label) {
        float size = rect.w > rect.h * 1.2f ? rect.h * 0.5f : rect.h * 0.44f;
        ph_touch_draw_label(renderer, label, rect, size, pressed);
    }
}

static void ph_touch_draw_pause(SDL_Renderer *renderer, const struct ph_touch *touch, SDL_FRect rect, int pressed)
{
    float bar = rect.h * 0.1f;
    float tall = rect.h * 0.36f;
    float x = rect.x + rect.w / 2;
    float y = rect.y + (rect.h - tall) / 2;
    ph_touch_draw_round(renderer, touch, rect, NULL, pressed);
    ph_fill_rect(renderer, (SDL_FRect){ x - bar * 1.6f, y, bar, tall }, ph_white(pressed ? 1 : 0.9f));
    ph_fill_rect(renderer, (SDL_FRect){ x + bar * 0.6f, y, bar, tall }, ph_white(pressed ? 1 : 0.9f));
}

static void ph_touch_draw_pad(SDL_Renderer *renderer, const struct ph_touch *touch, SDL_FRect rect, uint8_t directions)
{
    SDL_FPoint center = ph_rect_center(rect);
    float radius = rect.w / 2;
    float arm = radius * 0.32f;
    float reach = radius * 0.9f;
    float cx = center.x;
    float cy = center.y;
    SDL_FPoint cross[13] = {
        { cx - arm, cy - reach }, { cx + arm, cy - reach }, { cx + arm, cy - arm }, { cx + reach, cy - arm },
        { cx + reach, cy + arm }, { cx + arm, cy + arm }, { cx + arm, cy + reach }, { cx - arm, cy + reach },
        { cx - arm, cy + arm }, { cx - reach, cy + arm }, { cx - reach, cy - arm }, { cx - arm, cy - arm },
        { cx - arm, cy - reach },
    };
    ph_fill_circle(renderer, cx, cy, radius, ph_white(0.06f));
    ph_fill_fan(renderer, center, cross, 12, ph_white(0.12f));
    static const uint8_t ways[4] = { PH_TOUCH_UP, PH_TOUCH_DOWN, PH_TOUCH_LEFT, PH_TOUCH_RIGHT };
    static const float steps[4][2] = { { 0, -1 }, { 0, 1 }, { -1, 0 }, { 1, 0 } };
    for (int way = 0; way < 4; way++) {
        float dx = steps[way][0];
        float dy = steps[way][1];
        int pressed = (directions & ways[way]) != 0;
        if (pressed) {
            float x = dx ? (dx < 0 ? cx - reach : cx + arm) : cx - arm;
            float y = dy ? (dy < 0 ? cy - reach : cy + arm) : cy - arm;
            ph_fill_rect(renderer, (SDL_FRect){ x, y, dx ? reach - arm : arm * 2, dy ? reach - arm : arm * 2 }, ph_white(0.4f));
        }
        float tip = reach - arm * 0.45f;
        float size = arm * 0.5f;
        SDL_FPoint point = { cx + dx * tip, cy + dy * tip };
        SDL_FPoint left = { cx + dx * (tip - size) - dy * size, cy + dy * (tip - size) + dx * size };
        SDL_FPoint right = { cx + dx * (tip - size) + dy * size, cy + dy * (tip - size) - dx * size };
        ph_fill_triangle(renderer, point, left, right, ph_white(pressed ? 1 : 0.75f));
    }
    ph_polyline(renderer, cross, 13, ph_touch_line(touch), ph_white(0.6f));
}

static const struct ph_touch_finger *ph_touch_holder(const struct ph_touch *touch, int control)
{
    for (int slot = 0; slot < PH_TOUCH_FINGERS; slot++) {
        if (touch->fingers[slot].active && touch->fingers[slot].control == control) {
            return &touch->fingers[slot];
        }
    }
    return NULL;
}

static void ph_touch_draw_stick(SDL_Renderer *renderer, const struct ph_touch *touch, int control)
{
    SDL_FRect rect = touch->rects[control];
    const struct ph_touch_finger *finger = ph_touch_holder(touch, control);
    float radius = rect.w / 2;
    SDL_FPoint base = finger ? finger->origin : ph_rect_center(rect);
    float dx = finger ? finger->at.x - finger->origin.x : 0;
    float dy = finger ? finger->at.y - finger->origin.y : 0;
    float length = SDL_sqrtf(dx * dx + dy * dy);
    float travel = radius * 0.55f;
    if (length > travel) {
        dx *= travel / length;
        dy *= travel / length;
    }
    SDL_FRect well = ph_rect_around(base, radius * 2, radius * 2);
    SDL_FRect thumb = ph_rect_around((SDL_FPoint){ base.x + dx, base.y + dy }, radius * 0.9f, radius * 0.9f);
    ph_fill_round(renderer, well, radius, ph_white(finger ? 0.14f : 0.08f));
    ph_stroke_round(renderer, well, radius, ph_touch_line(touch), ph_white(finger ? 0.7f : 0.45f));
    ph_fill_round(renderer, thumb, thumb.w / 2, ph_white(finger ? 0.5f : 0.22f));
    ph_stroke_round(renderer, thumb, thumb.w / 2, ph_touch_line(touch), ph_white(finger ? 0.95f : 0.6f));
}

static void ph_touch_prepare_layer(struct ph_touch *touch, SDL_Renderer *renderer, SDL_FRect output)
{
    float width = 0;
    float height = 0;
    if (touch->layer && (!SDL_GetTextureSize(touch->layer, &width, &height) || SDL_GetRendererFromTexture(touch->layer) != renderer ||
                         (int)width != (int)output.w || (int)height != (int)output.h)) {
        SDL_DestroyTexture(touch->layer);
        touch->layer = NULL;
    }
    if (!touch->layer && output.w > 0 && output.h > 0) {
        touch->layer = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, (int)output.w, (int)output.h);
        if (touch->layer) {
            SDL_SetTextureBlendMode(touch->layer, SDL_BLENDMODE_BLEND_PREMULTIPLIED);
        }
    }
}

static void ph_touch_draw_controls(struct ph_touch *touch, SDL_Renderer *renderer)
{
    const struct ph_touch_control *controls = ph_touch_controls(touch);
    for (int index = 0; index < touch->count; index++) {
        if (!ph_touch_shown(touch, index)) {
            continue;
        }
        int pressed = touch->editing ? touch->grabbed == index : (touch->held & 1u << index) != 0;
        int kind = controls[index].kind;
        if (kind == PH_TOUCH_PAD) {
            ph_touch_draw_pad(renderer, touch, touch->rects[index], touch->editing ? (pressed ? 0x0F : 0) : touch->directions);
        } else if (kind == PH_TOUCH_STICK) {
            ph_touch_draw_stick(renderer, touch, index);
        } else if (kind == PH_TOUCH_PAUSE) {
            ph_touch_draw_pause(renderer, touch, touch->rects[index], pressed);
        } else {
            ph_touch_draw_round(renderer, touch, touch->rects[index], controls[index].label, pressed);
        }
    }
}

void ph_touch_draw(struct ph_touch *touch, SDL_Renderer *renderer)
{
    if (!ph_touch_visible(touch)) {
        return;
    }
    ph_touch_layout(touch, renderer);
    struct ph_render_state state = ph_overlay_begin(renderer);
    SDL_FRect output = ph_view_output(renderer);
    if (touch->editing) {
        ph_fill_rect(renderer, output, (SDL_FColor){ 0, 0, 0, 0.55f });
    }
    ph_touch_prepare_layer(touch, renderer, output);
    if (touch->layer) {
        SDL_Texture *target = SDL_GetRenderTarget(renderer);
        SDL_SetRenderTarget(renderer, touch->layer);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
        SDL_RenderClear(renderer);
        ph_touch_draw_controls(touch, renderer);
        SDL_SetRenderTarget(renderer, target);
        float alpha = touch->editing ? 1 : SDL_clamp(touch->opacity, 5, 100) / 100.0f;
        SDL_SetTextureColorModFloat(touch->layer, alpha, alpha, alpha);
        SDL_SetTextureAlphaModFloat(touch->layer, alpha);
        SDL_RenderTexture(renderer, touch->layer, NULL, NULL);
    }
    if (touch->editing) {
        ph_touch_draw_round(renderer, touch, touch->reset_rect, "Reset", touch->edit_pressed == PH_TOUCH_EDIT_RESET_BUTTON);
        ph_fill_round(renderer, touch->done_rect, touch->done_rect.h / 2, ph_white(touch->edit_pressed == PH_TOUCH_EDIT_DONE_BUTTON ? 0.75f : 0.95f));
        ph_text_centered(renderer, "Done", touch->done_rect.x + touch->done_rect.w / 2, touch->done_rect.y + touch->done_rect.h / 2, touch->done_rect.h * 0.5f, PH_TEXT_BOLD, (SDL_FColor){ 0.05f, 0.06f, 0.1f, 1 });
        float hint = 16 * ph_view_points(renderer);
        ph_text_centered(renderer, "Drag the controls to move them", touch->safe.x + touch->safe.w / 2, touch->done_rect.y + touch->done_rect.h + hint * 1.4f, hint, PH_TEXT_MEDIUM, ph_white(0.8f));
    }
    ph_overlay_end(renderer, state);
}

static int ph_touch_has_pad(const struct ph_touch *touch)
{
    for (int index = 0; index < touch->count; index++) {
        if (touch->layouts[PH_TOUCH_LANDSCAPE][index].kind == PH_TOUCH_PAD) {
            return 1;
        }
    }
    return 0;
}

int ph_touch_settings(struct ph_touch *touch, struct ph_setting *settings)
{
    settings[0] = (struct ph_setting){ .key = "touch_mode", .label = "Touch controls", .value = &touch->mode, .fallback = PH_TOUCH_AUTO, .choices = ph_touch_modes, .choice_count = 3 };
    settings[1] = (struct ph_setting){ .key = "touch_size", .label = "Touch size", .value = &touch->size, .fallback = 100, .minimum = 50, .maximum = 160, .step = 10, .unit = "%" };
    settings[2] = (struct ph_setting){ .key = "touch_opacity", .label = "Touch opacity", .value = &touch->opacity, .fallback = 70, .minimum = 20, .maximum = 100, .step = 10, .unit = "%" };
    settings[3] = (struct ph_setting){ .key = "touch_eight_way", .label = "Diagonals", .show = ph_touch_has_pad(touch) ? PH_SHOW_ALWAYS : PH_SHOW_HIDDEN, .value = &touch->eight_way, .choices = ph_off_on, .choice_count = 2 };
    settings[4] = (struct ph_setting){ .key = "touch_vibration", .label = "Vibration", .show = PH_SHOW_MOBILE, .value = &touch->vibration, .fallback = PH_MOBILE, .choices = ph_off_on, .choice_count = 2 };
    return PH_TOUCH_SETTINGS;
}

int ph_touch_take(struct ph_touch *touch, const char *key, const char *value)
{
    struct ph_setting settings[PH_TOUCH_SETTINGS];
    if (ph_settings_take(settings, ph_touch_settings(touch, settings), key, value)) {
        return 1;
    }
    const struct ph_touch_control *controls = touch->layouts[PH_TOUCH_LANDSCAPE];
    for (int orientation = 0; orientation < 2; orientation++) {
        size_t prefix = SDL_strlen(ph_orientation_keys[orientation]);
        if (SDL_strncmp(key, ph_orientation_keys[orientation], prefix)) {
            continue;
        }
        for (int index = 0; controls && index < touch->count; index++) {
            float x = 0;
            float y = 0;
            if (controls[index].key && !SDL_strcmp(key + prefix, controls[index].key)) {
                if (SDL_sscanf(value, "%f,%f", &x, &y) == 2) {
                    touch->places[orientation][index] = (SDL_FPoint){ SDL_clamp(x, 0.0f, 1.0f), SDL_clamp(y, 0.0f, 1.0f) };
                    touch->placed[orientation] |= 1u << index;
                }
                return 1;
            }
        }
        return 1;
    }
    return 0;
}

void ph_touch_write(SDL_IOStream *file, struct ph_touch *touch)
{
    struct ph_setting settings[PH_TOUCH_SETTINGS];
    ph_settings_write(file, settings, ph_touch_settings(touch, settings));
    const struct ph_touch_control *controls = touch->layouts[PH_TOUCH_LANDSCAPE];
    for (int orientation = 0; orientation < 2; orientation++) {
        for (int index = 0; controls && index < touch->count; index++) {
            if (controls[index].key && touch->placed[orientation] & 1u << index) {
                SDL_FPoint place = touch->places[orientation][index];
                SDL_IOprintf(file, "%s%s=%.4f,%.4f\n", ph_orientation_keys[orientation], controls[index].key, place.x, place.y);
            }
        }
    }
}

#endif
