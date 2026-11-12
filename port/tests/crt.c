#include <assert.h>
#include <math.h>
#include "port_crt.h"

int main(void) {
    assert(ph_crt_scanline_alpha(0, 480, 240, 0) == 0);
    assert(ph_crt_scanline_alpha(0, 0, 240, 2) == 0);
    assert(ph_crt_scanline_alpha(0, 480, 0, 2) == 0);

    uint8_t light = ph_crt_scanline_alpha(1, 480, 240, 1);
    uint8_t medium = ph_crt_scanline_alpha(1, 480, 240, 2);
    uint8_t heavy = ph_crt_scanline_alpha(1, 480, 240, 3);
    assert(ph_crt_scanline_alpha(0, 480, 240, 3) < heavy);
    assert(light > 0 && light < medium && medium < heavy);

    struct ph_crt crt = {.filter = PH_FILTER_CRT, .scanlines = 2, .mask = 0, .curvature = 1, .glow = 1};
    struct ph_crt_uniforms u = ph_crt_uniforms_for(&crt, 640.0f, 480.0f);
    assert(fabsf(u.source[0] - 640.0f) < 1e-5f);
    assert(fabsf(u.source[1] - 480.0f) < 1e-5f);
    assert(fabsf(u.source[2] - 1.0f / 640.0f) < 1e-6f);
    assert(fabsf(u.source[3] - 1.0f / 480.0f) < 1e-6f);
    assert(u.beam[0] < 0.0f && u.beam[3] > 0.0f);
    assert(fabsf(u.phosphors[0] - 1.0f) < 1e-5f);
    assert(u.tube[0] > 0.0f && u.tube[1] > 0.0f && u.tube[2] > 0.0f);

    crt.mask = 3;
    crt.curvature = 0;
    crt.glow = 0;
    crt.scanlines = 0;
    struct ph_crt_uniforms off = ph_crt_uniforms_for(&crt, 0.0f, -5.0f);
    assert(fabsf(off.source[0] - 1.0f) < 1e-5f && fabsf(off.source[1] - 1.0f) < 1e-5f);
    assert(fabsf(off.beam[0]) < 1e-5f && fabsf(off.beam[3]) < 1e-5f);
    assert(fabsf(off.phosphors[0]) < 1e-5f);
    assert(fabsf(off.tube[0]) < 1e-5f && fabsf(off.tube[2]) < 1e-5f);

    return 0;
}
