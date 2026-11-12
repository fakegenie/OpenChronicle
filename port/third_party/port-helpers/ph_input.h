#ifndef PH_INPUT_H
#define PH_INPUT_H

#include <SDL3/SDL.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    PH_PLAYERS = 4,
    PH_ACTIONS = 32,
    PH_SLOTS = 2,
    PH_BINDING_TEXT = 48,
};

enum {
    PH_SLOT_KEY,
    PH_SLOT_PAD,
};

enum {
    PH_BIND_NONE,
    PH_BIND_KEY,
    PH_BIND_BUTTON,
    PH_BIND_AXIS,
};

enum {
    PH_INPUT_IGNORED,
    PH_INPUT_CONSUMED,
    PH_INPUT_CHANGED,
};

struct ph_binding {
    int kind;
    int code;
    int direction;
};

struct ph_input {
    int players;
    int actions;
    const char *const *action_keys;
    const char *const *action_labels;
    struct ph_binding bindings[PH_PLAYERS][PH_ACTIONS][PH_SLOTS];
    struct ph_binding defaults[PH_PLAYERS][PH_ACTIONS][PH_SLOTS];
    SDL_Gamepad *pads[PH_PLAYERS];
    int dead_zone;
    int capturing;
    int capture_player;
    int capture_action;
    int capture_slot;
    uint32_t blocked[PH_PLAYERS];
};

void ph_input_init(struct ph_input *input, int players, int actions, const char *const *action_keys, const char *const *action_labels);
void ph_input_defaults(struct ph_input *input, int player, const int *keys, const int *buttons);
void ph_input_reset(struct ph_input *input, int player);
int ph_input_event(struct ph_input *input, const SDL_Event *event);
int ph_input_held(struct ph_input *input, int player, int action);
uint32_t ph_input_mask(struct ph_input *input, int player);
void ph_input_block_held(struct ph_input *input);
void ph_input_bind(struct ph_input *input, int player, int action, int slot, struct ph_binding binding);
void ph_input_capture(struct ph_input *input, int player, int action, int slot);
void ph_input_binding_text(const struct ph_input *input, int player, int action, int slot, char *text, int size);
const char *ph_input_pad_name(const struct ph_input *input, int player);
int ph_input_pad_count(const struct ph_input *input);
int ph_input_take(struct ph_input *input, const char *key, const char *value);
void ph_input_write(SDL_IOStream *file, const struct ph_input *input);

#ifdef __cplusplus
}
#endif

#endif

#if defined(PH_IMPLEMENTATION) && !defined(PH_INPUT_IMPLEMENTED)
#define PH_INPUT_IMPLEMENTED

enum {
    PH_DEAD_ZONE = 12000,
    PH_CAPTURE_AXIS = 24000,
};

static const char *const ph_slot_names[PH_SLOTS] = { "key", "pad" };
static const char *const ph_button_names[SDL_GAMEPAD_BUTTON_COUNT] = {
    "South", "East", "West", "North", "Back", "Guide", "Start", "Left Stick", "Right Stick", "LB", "RB",
    "D-Pad Up", "D-Pad Down", "D-Pad Left", "D-Pad Right", "Misc", "Paddle 1", "Paddle 2", "Paddle 3", "Paddle 4",
    "Touchpad", "Misc 2", "Misc 3", "Misc 4", "Misc 5", "Misc 6",
};
static const char *const ph_axis_names[SDL_GAMEPAD_AXIS_COUNT][2] = {
    { "Left Stick Left", "Left Stick Right" },
    { "Left Stick Up", "Left Stick Down" },
    { "Right Stick Left", "Right Stick Right" },
    { "Right Stick Up", "Right Stick Down" },
    { "LT", "LT" },
    { "RT", "RT" },
};

static int ph_pad_slot(const struct ph_input *input, SDL_JoystickID id)
{
    for (int player = 0; player < input->players; player++) {
        if (input->pads[player] && SDL_GetGamepadID(input->pads[player]) == id) {
            return player;
        }
    }
    return -1;
}

static void ph_open_pad(struct ph_input *input, SDL_JoystickID id)
{
    if (ph_pad_slot(input, id) >= 0) {
        return;
    }
    for (int player = 0; player < input->players; player++) {
        if (!input->pads[player]) {
            input->pads[player] = SDL_OpenGamepad(id);
            return;
        }
    }
}

void ph_input_init(struct ph_input *input, int players, int actions, const char *const *action_keys, const char *const *action_labels)
{
    SDL_zerop(input);
    input->players = SDL_clamp(players, 1, PH_PLAYERS);
    input->actions = SDL_clamp(actions, 1, PH_ACTIONS);
    input->action_keys = action_keys;
    input->action_labels = action_labels;
    input->dead_zone = PH_DEAD_ZONE;
    int count = 0;
    SDL_JoystickID *ids = SDL_GetGamepads(&count);
    for (int index = 0; ids && index < count; index++) {
        ph_open_pad(input, ids[index]);
    }
    SDL_free(ids);
}

void ph_input_defaults(struct ph_input *input, int player, const int *keys, const int *buttons)
{
    for (int action = 0; action < input->actions; action++) {
        struct ph_binding *slots = input->defaults[player][action];
        slots[PH_SLOT_KEY] = keys && keys[action] > 0 ? (struct ph_binding){ PH_BIND_KEY, keys[action], 0 } : (struct ph_binding){ PH_BIND_NONE, 0, 0 };
        slots[PH_SLOT_PAD] = buttons && buttons[action] >= 0 ? (struct ph_binding){ PH_BIND_BUTTON, buttons[action], 0 } : (struct ph_binding){ PH_BIND_NONE, 0, 0 };
    }
    ph_input_reset(input, player);
}

void ph_input_reset(struct ph_input *input, int player)
{
    SDL_memcpy(input->bindings[player], input->defaults[player], sizeof input->bindings[player]);
}

static int ph_same_binding(struct ph_binding left, struct ph_binding right)
{
    return left.kind == right.kind && left.code == right.code && left.direction == right.direction;
}

void ph_input_bind(struct ph_input *input, int player, int action, int slot, struct ph_binding binding)
{
    for (int other = 0; binding.kind != PH_BIND_NONE && other < input->players; other++) {
        if (binding.kind != PH_BIND_KEY && other != player) {
            continue;
        }
        for (int index = 0; index < input->actions; index++) {
            for (int place = 0; place < PH_SLOTS; place++) {
                if (ph_same_binding(input->bindings[other][index][place], binding)) {
                    input->bindings[other][index][place] = (struct ph_binding){ PH_BIND_NONE, 0, 0 };
                }
            }
        }
    }
    input->bindings[player][action][slot] = binding;
}

void ph_input_capture(struct ph_input *input, int player, int action, int slot)
{
    input->capturing = 1;
    input->capture_player = player;
    input->capture_action = action;
    input->capture_slot = slot;
}

static int ph_finish_capture(struct ph_input *input, struct ph_binding binding)
{
    input->capturing = 0;
    ph_input_bind(input, input->capture_player, input->capture_action, input->capture_slot, binding);
    return PH_INPUT_CHANGED;
}

static int ph_capture_event(struct ph_input *input, const SDL_Event *event)
{
    int pad = input->capture_slot == PH_SLOT_PAD;
    switch (event->type) {
    case SDL_EVENT_KEY_DOWN:
        if (event->key.repeat) {
            return PH_INPUT_CONSUMED;
        }
        if (event->key.scancode == SDL_SCANCODE_ESCAPE || event->key.scancode == SDL_SCANCODE_AC_BACK) {
            input->capturing = 0;
            return PH_INPUT_CONSUMED;
        }
        if (event->key.scancode == SDL_SCANCODE_DELETE || event->key.scancode == SDL_SCANCODE_BACKSPACE) {
            return ph_finish_capture(input, (struct ph_binding){ PH_BIND_NONE, 0, 0 });
        }
        return pad ? PH_INPUT_CONSUMED : ph_finish_capture(input, (struct ph_binding){ PH_BIND_KEY, event->key.scancode, 0 });
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        if (!pad || event->gbutton.button == SDL_GAMEPAD_BUTTON_GUIDE) {
            return PH_INPUT_CONSUMED;
        }
        if (event->gbutton.button == SDL_GAMEPAD_BUTTON_BACK) {
            input->capturing = 0;
            return PH_INPUT_CONSUMED;
        }
        return ph_finish_capture(input, (struct ph_binding){ PH_BIND_BUTTON, event->gbutton.button, 0 });
    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
        if (!pad || SDL_abs(event->gaxis.value) < PH_CAPTURE_AXIS) {
            return PH_INPUT_CONSUMED;
        }
        return ph_finish_capture(input, (struct ph_binding){ PH_BIND_AXIS, event->gaxis.axis, event->gaxis.value < 0 ? -1 : 1 });
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_FINGER_DOWN:
        input->capturing = 0;
        return PH_INPUT_CONSUMED;
    default:
        return PH_INPUT_IGNORED;
    }
}

int ph_input_event(struct ph_input *input, const SDL_Event *event)
{
    if (event->type == SDL_EVENT_GAMEPAD_ADDED) {
        ph_open_pad(input, event->gdevice.which);
        return PH_INPUT_IGNORED;
    }
    if (event->type == SDL_EVENT_GAMEPAD_REMOVED) {
        int player = ph_pad_slot(input, event->gdevice.which);
        if (player >= 0) {
            SDL_CloseGamepad(input->pads[player]);
            input->pads[player] = NULL;
        }
        return PH_INPUT_IGNORED;
    }
    return input->capturing ? ph_capture_event(input, event) : PH_INPUT_IGNORED;
}

static int ph_stick_direction(SDL_Gamepad *pad, int button, int dead_zone)
{
    int x = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTX);
    int y = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTY);
    switch (button) {
    case SDL_GAMEPAD_BUTTON_DPAD_UP:
        return y < -dead_zone;
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
        return y > dead_zone;
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
        return x < -dead_zone;
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
        return x > dead_zone;
    default:
        return 0;
    }
}

static int ph_binding_held(const struct ph_input *input, int player, struct ph_binding binding)
{
    SDL_Gamepad *pad = input->pads[player];
    if (binding.kind == PH_BIND_KEY) {
        int count = 0;
        const bool *keys = SDL_GetKeyboardState(&count);
        return binding.code > 0 && binding.code < count && keys[binding.code];
    }
    if (binding.kind == PH_BIND_BUTTON && pad) {
        return SDL_GetGamepadButton(pad, (SDL_GamepadButton)binding.code) || ph_stick_direction(pad, binding.code, input->dead_zone);
    }
    if (binding.kind == PH_BIND_AXIS && pad) {
        int value = SDL_GetGamepadAxis(pad, (SDL_GamepadAxis)binding.code);
        return (binding.direction < 0 ? -value : value) > input->dead_zone;
    }
    return 0;
}

static int ph_input_down(const struct ph_input *input, int player, int action)
{
    for (int slot = 0; slot < PH_SLOTS; slot++) {
        if (ph_binding_held(input, player, input->bindings[player][action][slot])) {
            return 1;
        }
    }
    return 0;
}

int ph_input_held(struct ph_input *input, int player, int action)
{
    uint32_t bit = 1u << action;
    if (!ph_input_down(input, player, action)) {
        input->blocked[player] &= ~bit;
        return 0;
    }
    return !(input->blocked[player] & bit);
}

void ph_input_block_held(struct ph_input *input)
{
    for (int player = 0; player < input->players; player++) {
        input->blocked[player] = 0;
        for (int action = 0; action < input->actions; action++) {
            input->blocked[player] |= ph_input_down(input, player, action) ? 1u << action : 0;
        }
    }
}

uint32_t ph_input_mask(struct ph_input *input, int player)
{
    uint32_t mask = 0;
    for (int action = 0; action < input->actions; action++) {
        mask |= ph_input_held(input, player, action) ? 1u << action : 0;
    }
    return mask;
}

static void ph_binding_name(const struct ph_input *input, int player, struct ph_binding binding, char *text, int size)
{
    if (binding.kind == PH_BIND_KEY) {
        const char *name = SDL_GetScancodeName((SDL_Scancode)binding.code);
        SDL_strlcpy(text, name && *name ? name : "None", (size_t)size);
    } else if (binding.kind == PH_BIND_BUTTON && binding.code >= 0 && binding.code < SDL_GAMEPAD_BUTTON_COUNT) {
        SDL_GamepadButtonLabel label = input->pads[player] ? SDL_GetGamepadButtonLabel(input->pads[player], (SDL_GamepadButton)binding.code) : SDL_GAMEPAD_BUTTON_LABEL_UNKNOWN;
        static const char *const labels[] = { "", "A", "B", "X", "Y", "Cross", "Circle", "Square", "Triangle" };
        SDL_strlcpy(text, label > SDL_GAMEPAD_BUTTON_LABEL_UNKNOWN && label <= SDL_GAMEPAD_BUTTON_LABEL_TRIANGLE ? labels[label] : ph_button_names[binding.code], (size_t)size);
    } else if (binding.kind == PH_BIND_AXIS && binding.code >= 0 && binding.code < SDL_GAMEPAD_AXIS_COUNT) {
        SDL_strlcpy(text, ph_axis_names[binding.code][binding.direction > 0], (size_t)size);
    } else {
        SDL_strlcpy(text, "None", (size_t)size);
    }
}

void ph_input_binding_text(const struct ph_input *input, int player, int action, int slot, char *text, int size)
{
    ph_binding_name(input, player, input->bindings[player][action][slot], text, size);
}

const char *ph_input_pad_name(const struct ph_input *input, int player)
{
    return input->pads[player] ? SDL_GetGamepadName(input->pads[player]) : NULL;
}

int ph_input_pad_count(const struct ph_input *input)
{
    int count = 0;
    for (int player = 0; player < input->players; player++) {
        count += input->pads[player] != NULL;
    }
    return count;
}

static int ph_parse_binding(const char *value, struct ph_binding *binding)
{
    const char *colon = SDL_strchr(value, ':');
    *binding = (struct ph_binding){ PH_BIND_NONE, 0, 0 };
    if (!colon) {
        return !SDL_strcasecmp(value, "none");
    }
    const char *name = colon + 1;
    size_t kind = (size_t)(colon - value);
    if (kind == 3 && !SDL_strncmp(value, "key", 3)) {
        SDL_Scancode code = SDL_GetScancodeFromName(name);
        *binding = (struct ph_binding){ code ? PH_BIND_KEY : PH_BIND_NONE, code, 0 };
        return 1;
    }
    if (kind == 6 && !SDL_strncmp(value, "button", 6)) {
        SDL_GamepadButton code = SDL_GetGamepadButtonFromString(name);
        if (code == SDL_GAMEPAD_BUTTON_GUIDE || code == SDL_GAMEPAD_BUTTON_BACK) {
            return 0;
        }
        *binding = (struct ph_binding){ code != SDL_GAMEPAD_BUTTON_INVALID ? PH_BIND_BUTTON : PH_BIND_NONE, code, 0 };
        return 1;
    }
    if (kind == 4 && !SDL_strncmp(value, "axis", 4)) {
        char axis[32];
        SDL_strlcpy(axis, name, sizeof axis);
        size_t length = SDL_strlen(axis);
        int direction = length && axis[length - 1] == '-' ? -1 : 1;
        if (length && (axis[length - 1] == '-' || axis[length - 1] == '+')) {
            axis[length - 1] = 0;
        }
        SDL_GamepadAxis code = SDL_GetGamepadAxisFromString(axis);
        *binding = (struct ph_binding){ code != SDL_GAMEPAD_AXIS_INVALID ? PH_BIND_AXIS : PH_BIND_NONE, code, direction };
        return 1;
    }
    return 0;
}

int ph_input_take(struct ph_input *input, const char *key, const char *value)
{
    for (int player = 0; player < input->players; player++) {
        for (int action = 0; action < input->actions; action++) {
            for (int slot = 0; slot < PH_SLOTS; slot++) {
                char name[64];
                SDL_snprintf(name, sizeof name, "p%d_%s_%s", player + 1, input->action_keys[action], ph_slot_names[slot]);
                struct ph_binding binding;
                if (!SDL_strcmp(name, key)) {
                    if (ph_parse_binding(value, &binding)) {
                        input->bindings[player][action][slot] = binding;
                    }
                    return 1;
                }
            }
        }
    }
    return 0;
}

void ph_input_write(SDL_IOStream *file, const struct ph_input *input)
{
    for (int player = 0; player < input->players; player++) {
        for (int action = 0; action < input->actions; action++) {
            for (int slot = 0; slot < PH_SLOTS; slot++) {
                struct ph_binding binding = input->bindings[player][action][slot];
                const char *key = input->action_keys[action];
                const char *slot_name = ph_slot_names[slot];
                if (binding.kind == PH_BIND_KEY) {
                    SDL_IOprintf(file, "p%d_%s_%s=key:%s\n", player + 1, key, slot_name, SDL_GetScancodeName((SDL_Scancode)binding.code));
                } else if (binding.kind == PH_BIND_BUTTON) {
                    SDL_IOprintf(file, "p%d_%s_%s=button:%s\n", player + 1, key, slot_name, SDL_GetGamepadStringForButton((SDL_GamepadButton)binding.code));
                } else if (binding.kind == PH_BIND_AXIS) {
                    SDL_IOprintf(file, "p%d_%s_%s=axis:%s%c\n", player + 1, key, slot_name, SDL_GetGamepadStringForAxis((SDL_GamepadAxis)binding.code), binding.direction < 0 ? '-' : '+');
                } else {
                    SDL_IOprintf(file, "p%d_%s_%s=none\n", player + 1, key, slot_name);
                }
            }
        }
    }
}

#endif
