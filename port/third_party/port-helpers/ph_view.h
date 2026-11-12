#ifndef PH_VIEW_H
#define PH_VIEW_H

#include <SDL3/SDL.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(SDL_PLATFORM_IOS) || defined(SDL_PLATFORM_ANDROID)
#define PH_MOBILE 1
#else
#define PH_MOBILE 0
#endif

enum {
    PH_SCALE_INTEGER,
    PH_SCALE_FIT,
    PH_SCALE_STRETCH,
};

SDL_FRect ph_view_fit(SDL_FRect area, float width, float height, float pixel_aspect, int scaling);
float ph_tv_pixel_aspect(float width, float height);
void ph_view_set_window(SDL_Renderer *renderer, SDL_Window *window);
SDL_FRect ph_view_output(SDL_Renderer *renderer);
SDL_FRect ph_view_safe(SDL_Renderer *renderer);
float ph_view_density(SDL_Renderer *renderer);
float ph_view_points(SDL_Renderer *renderer);
int ph_view_event_point(SDL_Renderer *renderer, const SDL_Event *event, SDL_FPoint *point);
void ph_view_window_size(SDL_Window *window, float width, float height, float pixel_aspect, int scale);
int ph_view_fullscreen_key(const SDL_Event *event);
void ph_view_prefer_landscape(void);

#ifdef __cplusplus
}
#endif

#endif

#if defined(PH_IMPLEMENTATION) && !defined(PH_VIEW_IMPLEMENTED)
#define PH_VIEW_IMPLEMENTED

enum {
    PH_VIEW_TITLE_BAR = 32,
};

SDL_FRect ph_view_fit(SDL_FRect area, float width, float height, float pixel_aspect, int scaling)
{
    if (area.w <= 0 || area.h <= 0 || width <= 0 || height <= 0) {
        return area;
    }
    if (scaling == PH_SCALE_STRETCH) {
        return area;
    }
    float shown_width = width * (pixel_aspect > 0 ? pixel_aspect : 1);
    float scale = SDL_min(area.w / shown_width, area.h / height);
    if (scaling == PH_SCALE_INTEGER && scale >= 1) {
        scale = SDL_floorf(scale);
    }
    float fitted_width = SDL_floorf(shown_width * scale);
    float fitted_height = SDL_floorf(height * scale);
    return (SDL_FRect){ SDL_floorf(area.x + (area.w - fitted_width) / 2), SDL_floorf(area.y + (area.h - fitted_height) / 2), fitted_width, fitted_height };
}

float ph_tv_pixel_aspect(float width, float height)
{
    return width > 0 && height > 0 ? 4.0f / 3.0f * height / width : 1;
}

SDL_FRect ph_view_output(SDL_Renderer *renderer)
{
    int width = 0;
    int height = 0;
    SDL_GetRenderOutputSize(renderer, &width, &height);
    return (SDL_FRect){ 0, 0, (float)width, (float)height };
}

void ph_view_set_window(SDL_Renderer *renderer, SDL_Window *window)
{
    if (renderer) {
        SDL_SetPointerProperty(SDL_GetRendererProperties(renderer), "ph.window", window);
    }
}

static SDL_Window *ph_view_window(SDL_Renderer *renderer)
{
    if (!renderer) {
        return NULL;
    }
    SDL_Window *window = SDL_GetRenderWindow(renderer);
    if (window) {
        return window;
    }
    return (SDL_Window *)SDL_GetPointerProperty(SDL_GetRendererProperties(renderer), "ph.window", NULL);
}

float ph_view_density(SDL_Renderer *renderer)
{
    SDL_Window *window = ph_view_window(renderer);
    int window_width = 0;
    int window_height = 0;
    SDL_FRect output = ph_view_output(renderer);
    if (!window || !SDL_GetWindowSize(window, &window_width, &window_height) || window_width <= 0 || output.w <= 0) {
        return 1;
    }
    return output.w / (float)window_width;
}

float ph_view_points(SDL_Renderer *renderer)
{
    SDL_Window *window = ph_view_window(renderer);
    float scale = window ? SDL_GetWindowDisplayScale(window) : 0;
    return scale > 0 ? scale : ph_view_density(renderer);
}

SDL_FRect ph_view_safe(SDL_Renderer *renderer)
{
    SDL_Window *window = ph_view_window(renderer);
    SDL_FRect output = ph_view_output(renderer);
    SDL_Rect safe;
    if (!window || !SDL_GetWindowSafeArea(window, &safe) || safe.w <= 0 || safe.h <= 0) {
        return output;
    }
    float density = ph_view_density(renderer);
    return (SDL_FRect){ safe.x * density, safe.y * density, safe.w * density, safe.h * density };
}

int ph_view_event_point(SDL_Renderer *renderer, const SDL_Event *event, SDL_FPoint *point)
{
    SDL_FRect output = ph_view_output(renderer);
    float density = ph_view_density(renderer);
    switch (event->type) {
    case SDL_EVENT_FINGER_DOWN:
    case SDL_EVENT_FINGER_UP:
    case SDL_EVENT_FINGER_MOTION:
    case SDL_EVENT_FINGER_CANCELED:
        *point = (SDL_FPoint){ event->tfinger.x * output.w, event->tfinger.y * output.h };
        return 1;
    case SDL_EVENT_MOUSE_MOTION:
        *point = (SDL_FPoint){ event->motion.x * density, event->motion.y * density };
        return 1;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
        *point = (SDL_FPoint){ event->button.x * density, event->button.y * density };
        return 1;
    case SDL_EVENT_MOUSE_WHEEL:
        *point = (SDL_FPoint){ event->wheel.mouse_x * density, event->wheel.mouse_y * density };
        return 1;
    default:
        return 0;
    }
}

void ph_view_window_size(SDL_Window *window, float width, float height, float pixel_aspect, int scale)
{
    SDL_DisplayID display = SDL_GetDisplayForWindow(window);
    float shown_width = width * (pixel_aspect > 0 ? pixel_aspect : 1);
    SDL_Rect usable;
    if (SDL_GetDisplayUsableBounds(display, &usable)) {
        while (scale > 1 && (shown_width * scale > usable.w || height * scale + PH_VIEW_TITLE_BAR > usable.h)) {
            scale--;
        }
    }
    SDL_SetWindowSize(window, (int)SDL_roundf(shown_width * scale), (int)SDL_roundf(height * scale));
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED_DISPLAY(display), SDL_WINDOWPOS_CENTERED_DISPLAY(display));
}

int ph_view_fullscreen_key(const SDL_Event *event)
{
    if (PH_MOBILE || event->type != SDL_EVENT_KEY_DOWN || event->key.repeat) {
        return 0;
    }
    SDL_Scancode key = event->key.scancode;
    int enter = key == SDL_SCANCODE_RETURN || key == SDL_SCANCODE_KP_ENTER;
    return key == SDL_SCANCODE_F11 || (enter && (event->key.mod & (SDL_KMOD_ALT | SDL_KMOD_GUI)));
}

void ph_view_prefer_landscape(void)
{
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
}

#endif
