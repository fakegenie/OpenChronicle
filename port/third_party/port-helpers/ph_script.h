#ifndef PH_SCRIPT_H
#define PH_SCRIPT_H

#include <SDL3/SDL.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    PH_SCRIPT_LINES = 256,
    PH_SCRIPT_TEXT = 128,
};

enum {
    PH_SCRIPT_KEY,
    PH_SCRIPT_TYPE,
    PH_SCRIPT_CLICK,
    PH_SCRIPT_TAP,
    PH_SCRIPT_SHOT,
    PH_SCRIPT_SIZE,
    PH_SCRIPT_QUIT,
    PH_SCRIPT_DROP,
    PH_SCRIPT_DOWN,
    PH_SCRIPT_UP,
};

struct ph_script_line {
    long frame;
    int command;
    int x;
    int y;
    char text[PH_SCRIPT_TEXT];
};

struct ph_script {
    struct ph_script_line lines[PH_SCRIPT_LINES];
    int count;
    int next;
    long frame;
    char text[PH_SCRIPT_TEXT];
};

int ph_script_load(struct ph_script *script, const char *path);
void ph_script_step(struct ph_script *script, SDL_Window *window, SDL_Renderer *renderer);

#ifdef __cplusplus
}
#endif

#endif

#if defined(PH_IMPLEMENTATION) && !defined(PH_SCRIPT_IMPLEMENTED)
#define PH_SCRIPT_IMPLEMENTED

static const char *const ph_script_commands[] = { "key", "text", "click", "tap", "shot", "size", "quit", "drop", "down", "up" };

static void ph_script_key(SDL_Window *window, const char *name, int down)
{
    SDL_Scancode code = SDL_GetScancodeFromName(name);
    SDL_Event event;
    SDL_zero(event);
    event.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    event.key.windowID = SDL_GetWindowID(window);
    event.key.scancode = code;
    event.key.key = SDL_GetKeyFromScancode(code, SDL_KMOD_NONE, false);
    event.key.down = down;
    SDL_PushEvent(&event);
}

static void ph_script_click(SDL_Window *window, int x, int y)
{
    SDL_Event event;
    SDL_zero(event);
    event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    event.button.windowID = SDL_GetWindowID(window);
    event.button.button = SDL_BUTTON_LEFT;
    event.button.down = true;
    event.button.clicks = 1;
    event.button.x = (float)x;
    event.button.y = (float)y;
    SDL_PushEvent(&event);
    event.type = SDL_EVENT_MOUSE_BUTTON_UP;
    event.button.down = false;
    SDL_PushEvent(&event);
}

static void ph_script_tap(SDL_Window *window, int x, int y)
{
    int width = 0;
    int height = 0;
    SDL_GetWindowSize(window, &width, &height);
    SDL_Event event;
    SDL_zero(event);
    event.type = SDL_EVENT_FINGER_DOWN;
    event.tfinger.windowID = SDL_GetWindowID(window);
    event.tfinger.touchID = 1;
    event.tfinger.fingerID = 1;
    event.tfinger.x = width > 0 ? (float)x / width : 0;
    event.tfinger.y = height > 0 ? (float)y / height : 0;
    event.tfinger.pressure = 1;
    SDL_PushEvent(&event);
    event.type = SDL_EVENT_FINGER_UP;
    event.tfinger.pressure = 0;
    SDL_PushEvent(&event);
}

static void ph_script_shot(SDL_Renderer *renderer, const char *path)
{
    SDL_Surface *surface = SDL_RenderReadPixels(renderer, NULL);
    if (!surface) {
        return;
    }
    size_t length = SDL_strlen(path);
    if (length > 4 && !SDL_strcasecmp(path + length - 4, ".png")) {
        SDL_SavePNG(surface, path);
    } else {
        SDL_SaveBMP(surface, path);
    }
    SDL_DestroySurface(surface);
}

static void ph_script_push_text(struct ph_script *script, SDL_Window *window, int type, const char *text)
{
    SDL_Event event;
    SDL_zero(event);
    SDL_strlcpy(script->text, text, sizeof script->text);
    event.type = (Uint32)type;
    if (type == SDL_EVENT_TEXT_INPUT) {
        event.text.windowID = SDL_GetWindowID(window);
        event.text.text = script->text;
    } else {
        event.drop.windowID = SDL_GetWindowID(window);
        event.drop.data = script->text;
    }
    SDL_PushEvent(&event);
}

static void ph_script_run(struct ph_script *script, const struct ph_script_line *line, SDL_Window *window, SDL_Renderer *renderer)
{
    SDL_Event event;
    SDL_zero(event);
    switch (line->command) {
    case PH_SCRIPT_KEY:
        ph_script_key(window, line->text, 1);
        ph_script_key(window, line->text, 0);
        break;
    case PH_SCRIPT_DOWN:
        ph_script_key(window, line->text, 1);
        break;
    case PH_SCRIPT_UP:
        ph_script_key(window, line->text, 0);
        break;
    case PH_SCRIPT_TYPE:
        ph_script_push_text(script, window, SDL_EVENT_TEXT_INPUT, line->text);
        break;
    case PH_SCRIPT_CLICK:
        ph_script_click(window, line->x, line->y);
        break;
    case PH_SCRIPT_TAP:
        ph_script_tap(window, line->x, line->y);
        break;
    case PH_SCRIPT_SHOT:
        ph_script_shot(renderer, line->text);
        break;
    case PH_SCRIPT_SIZE:
        SDL_SetWindowSize(window, line->x, line->y);
        break;
    case PH_SCRIPT_QUIT:
        event.type = SDL_EVENT_QUIT;
        SDL_PushEvent(&event);
        break;
    case PH_SCRIPT_DROP:
        ph_script_push_text(script, window, SDL_EVENT_DROP_FILE, line->text);
        break;
    }
}

static int ph_script_parse(const char *line, struct ph_script_line *parsed)
{
    char name[16];
    int offset = 0;
    SDL_zerop(parsed);
    if (SDL_sscanf(line, "%ld %15s %n", &parsed->frame, name, &offset) < 2) {
        return 0;
    }
    SDL_strlcpy(parsed->text, line + offset, sizeof parsed->text);
    size_t length = SDL_strlen(parsed->text);
    while (length && (parsed->text[length - 1] == '\n' || parsed->text[length - 1] == '\r')) {
        parsed->text[--length] = 0;
    }
    parsed->command = -1;
    for (int index = 0; index < (int)SDL_arraysize(ph_script_commands); index++) {
        if (!SDL_strcmp(name, ph_script_commands[index])) {
            parsed->command = index;
        }
    }
    if (parsed->command == PH_SCRIPT_CLICK || parsed->command == PH_SCRIPT_TAP || parsed->command == PH_SCRIPT_SIZE) {
        return SDL_sscanf(parsed->text, "%d %d", &parsed->x, &parsed->y) == 2;
    }
    return parsed->command >= 0;
}

int ph_script_load(struct ph_script *script, const char *path)
{
    char *text = SDL_LoadFile(path, NULL);
    if (!text) {
        return 0;
    }
    SDL_zerop(script);
    char *line = text;
    while (line && *line && script->count < PH_SCRIPT_LINES) {
        char *end = SDL_strchr(line, '\n');
        if (end) {
            *end = 0;
        }
        script->count += ph_script_parse(line, &script->lines[script->count]);
        line = end ? end + 1 : NULL;
    }
    SDL_free(text);
    return 1;
}

void ph_script_step(struct ph_script *script, SDL_Window *window, SDL_Renderer *renderer)
{
    script->frame++;
    while (script->next < script->count && script->lines[script->next].frame <= script->frame) {
        ph_script_run(script, &script->lines[script->next++], window, renderer);
    }
}

#endif
