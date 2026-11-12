#if defined(PH_IMPLEMENTATION) && defined(__linux__) && !defined(_DEFAULT_SOURCE)
#define _DEFAULT_SOURCE
#endif

#define PH_VERSION 2

#include "ph_hash.h"
#include "ph_zip.h"
#include "ph_files.h"
#include "ph_view.h"
#include "ph_draw.h"
#include "ph_settings.h"
#include "ph_crt.h"
#include "ph_input.h"
#include "ph_ios.h"
#include "ph_touch.h"
#include "ph_menu.h"
#include "ph_net.h"
#include "ph_config.h"
#include "ph_debug.h"
#include "ph_launcher.h"
#include "ph_script.h"
