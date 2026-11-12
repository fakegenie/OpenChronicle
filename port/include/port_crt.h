#pragma once

#include "ph_crt.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef PH_CRT_IMPLEMENTED
struct ph_crt_uniforms {
    float source[4];
    float beam[4];
    float phosphors[4];
    float tube[4];
};
#endif

void render_set_crt(int filter, int scanlines, int mask, int curvature, int glow);
void render_apply_crt_settings(void);

#ifdef __cplusplus
}
#endif
