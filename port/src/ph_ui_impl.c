#if defined(__linux__) && !defined(_DEFAULT_SOURCE)
#define _DEFAULT_SOURCE
#endif

#define PH_CRT_IMPLEMENTED
#define PH_SETTINGS_IMPLEMENTED
#define PH_VIEW_IMPLEMENTED
#define PH_IMPLEMENTATION
#include "ph_files.h"
#include "ph_view.h"
#include "ph_draw.h"
#include "ph_settings.h"
#include "ph_input.h"
#include "ph_ios.h"
#include "ph_touch.h"
#include "ph_menu.h"
#include "ph_net.h"
#include "ph_config.h"
#include "ph_debug.h"
#include "ph_launcher.h"

#ifdef SDL_PLATFORM_IOS
#include "ios_files.h"

int ph_ios_pick_file(SDL_Window *window)
{
    char path[PH_PATH_SIZE] = "";
    port_ios_pick_disc(window, path, sizeof path);
    if (path[0]) {
        ph_file_picked(path);
        return 1;
    }
    return 0;
}
#endif
