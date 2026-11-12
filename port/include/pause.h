#pragma once

#include "launcher.h"
#include "touch.h"
#include <SDL3/SDL.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    PAUSE_NONE,
    PAUSE_RESUME,
    PAUSE_SAVE_STATE,
    PAUSE_LOAD_STATE,
    PAUSE_QUIT,
};

void port_pause_open(int opened_by_chord);
void port_pause_close(void);
int port_pause_active(void);
void port_pause_set_status(const char *message);
int port_pause_event(const SDL_Event *event);
int port_pause_tick(void);
const LauncherPicture *port_pause_atlas(int pixel_width, int pixel_height);
uint64_t port_pause_atlas_serial(void);
int port_pause_shapes(TouchShape *shapes, int limit, int pixel_width, int pixel_height, SDL_Rect *clip, int *clip_start);

#ifdef __cplusplus
}
#endif
