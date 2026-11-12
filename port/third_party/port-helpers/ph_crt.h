#ifndef PH_CRT_H
#define PH_CRT_H

#include <SDL3/SDL.h>

#include "ph_settings.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    PH_FILTER_SHARP,
    PH_FILTER_NEAREST,
    PH_FILTER_SMOOTH,
    PH_FILTER_CRT,
};

enum {
    PH_CRT_SETTINGS = 5,
};

struct ph_crt {
    int filter;
    int scanlines;
    int mask;
    int curvature;
    int glow;
    SDL_Renderer *renderer;
    SDL_GPUShader *shader;
    SDL_GPURenderState *state;
    SDL_Texture *lines;
    int line_rows;
    int line_source;
    int line_level;
};

SDL_Renderer *ph_crt_create_renderer(SDL_Window *window);
SDL_GPUShader *ph_crt_create_shader(SDL_GPUDevice *device);
struct ph_crt_uniforms ph_crt_uniforms_for(const struct ph_crt *crt, float width, float height);
uint8_t ph_crt_scanline_alpha(int row, int rows, int source, int scanlines);
void ph_crt_open(struct ph_crt *crt, SDL_Renderer *renderer);
void ph_crt_close(struct ph_crt *crt);
int ph_crt_settings(struct ph_crt *crt, struct ph_setting *settings, int section);
void ph_crt_draw(struct ph_crt *crt, SDL_Texture *picture, SDL_FRect area);

#ifdef __cplusplus
}
#endif

#endif

#if defined(PH_IMPLEMENTATION) && !defined(PH_CRT_IMPLEMENTED)
#define PH_CRT_IMPLEMENTED

#include "ph_crt_shader.h"

enum {
    PH_CRT_LINE_SAMPLES = 8,
    PH_CRT_ALPHA_SHIFT = 24,
    PH_CRT_ALPHA_MAX = 255,
};

struct ph_crt_uniforms {
    float source[4];
    float beam[4];
    float phosphors[4];
    float tube[4];
};

static const char *const ph_crt_filters[] = { "Sharp", "Nearest", "Smooth", "CRT" };
static const char *const ph_crt_scanline_names[] = { "Off", "Light", "Medium", "Heavy" };
static const char *const ph_crt_masks[] = { "Aperture grille", "Slot mask", "Shadow mask", "Off" };
static const char *const ph_crt_curvatures[] = { "Off", "Slight", "Strong" };
static const char *const ph_crt_glows[] = { "Off", "Soft", "Strong" };

static const SDL_ScaleMode ph_crt_scale_modes[] = { SDL_SCALEMODE_PIXELART, SDL_SCALEMODE_NEAREST, SDL_SCALEMODE_LINEAR, SDL_SCALEMODE_NEAREST };
static const float ph_crt_line_strengths[] = { 0.0f, 0.3f, 0.5f, 0.7f };
static const float ph_crt_scan_hardness[] = { 0.0f, -5.0f, -8.0f, -12.0f };
static const float ph_crt_bright_scan_softening = 0.55f;
static const float ph_crt_pixel_hardness = -3.0f;
static const float ph_crt_glow_levels[] = { 0.0f, 0.08f, 0.18f };
static const float ph_crt_mask_types[] = { 1.0f, 2.0f, 3.0f, 0.0f };
static const float ph_crt_mask_dark = 0.65f;
static const float ph_crt_mask_light = 1.35f;
static const float ph_crt_brightness[] = { 1.12f, 1.2f, 1.28f, 1.38f };
static const float ph_crt_warps[][2] = { { 0.0f, 0.0f }, { 0.031f, 0.041f }, { 0.07f, 0.09f } };
static const float ph_crt_corners[] = { 0.0f, 6.0f, 10.0f };
static const float ph_crt_line_dark_from = 0.4f;
static const float ph_crt_line_dark_ramp = 0.2f;

SDL_Renderer *ph_crt_create_renderer(SDL_Window *window)
{
    SDL_PropertiesID properties = SDL_CreateProperties();
    SDL_SetPointerProperty(properties, SDL_PROP_RENDERER_CREATE_WINDOW_POINTER, window);
    SDL_SetStringProperty(properties, SDL_PROP_RENDERER_CREATE_NAME_STRING, SDL_GPU_RENDERER);
    SDL_SetBooleanProperty(properties, SDL_PROP_RENDERER_CREATE_GPU_SHADERS_SPIRV_BOOLEAN, true);
    SDL_SetBooleanProperty(properties, SDL_PROP_RENDERER_CREATE_GPU_SHADERS_MSL_BOOLEAN, true);
    SDL_Renderer *renderer = SDL_CreateRendererWithProperties(properties);
    SDL_DestroyProperties(properties);
    return renderer ? renderer : SDL_CreateRenderer(window, NULL);
}

SDL_GPUShader *ph_crt_create_shader(SDL_GPUDevice *device)
{
    SDL_GPUShaderFormat formats = device ? SDL_GetGPUShaderFormats(device) : SDL_GPU_SHADERFORMAT_INVALID;
    SDL_GPUShaderCreateInfo shader = {
        .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
        .num_samplers = 1,
        .num_uniform_buffers = 1,
    };
    if (formats & SDL_GPU_SHADERFORMAT_SPIRV) {
        shader.code = ph_crt_spirv;
        shader.code_size = sizeof ph_crt_spirv;
        shader.entrypoint = "main";
        shader.format = SDL_GPU_SHADERFORMAT_SPIRV;
        return SDL_CreateGPUShader(device, &shader);
    }
    if (formats & SDL_GPU_SHADERFORMAT_MSL) {
        shader.code = ph_crt_msl;
        shader.code_size = sizeof ph_crt_msl;
        shader.entrypoint = "main0";
        shader.format = SDL_GPU_SHADERFORMAT_MSL;
        return SDL_CreateGPUShader(device, &shader);
    }
    return NULL;
}

void ph_crt_open(struct ph_crt *crt, SDL_Renderer *renderer)
{
    crt->renderer = renderer;
    crt->shader = ph_crt_create_shader(SDL_GetGPURendererDevice(renderer));
    if (crt->shader) {
        SDL_GPURenderStateCreateInfo state = { .fragment_shader = crt->shader };
        crt->state = SDL_CreateGPURenderState(renderer, &state);
    }
}

void ph_crt_close(struct ph_crt *crt)
{
    SDL_DestroyGPURenderState(crt->state);
    if (crt->shader && crt->renderer) {
        SDL_ReleaseGPUShader(SDL_GetGPURendererDevice(crt->renderer), crt->shader);
    }
    SDL_DestroyTexture(crt->lines);
    crt->state = NULL;
    crt->shader = NULL;
    crt->lines = NULL;
    crt->line_rows = 0;
}

int ph_crt_settings(struct ph_crt *crt, struct ph_setting *settings, int section)
{
    int shown = crt->filter == PH_FILTER_CRT && (crt->state || crt->shader) ? PH_SHOW_ALWAYS : PH_SHOW_HIDDEN;
    settings[0] = (struct ph_setting){ .key = "filter", .label = "Filter", .section = section, .value = &crt->filter, .choices = ph_crt_filters, .choice_count = SDL_arraysize(ph_crt_filters) };
    settings[1] = (struct ph_setting){ .key = "scanlines", .label = "Scanlines", .section = section, .value = &crt->scanlines, .choices = ph_crt_scanline_names, .choice_count = SDL_arraysize(ph_crt_scanline_names) };
    settings[2] = (struct ph_setting){ .key = "crt_mask", .label = "CRT mask", .section = section, .show = shown, .value = &crt->mask, .choices = ph_crt_masks, .choice_count = SDL_arraysize(ph_crt_masks) };
    settings[3] = (struct ph_setting){ .key = "crt_curvature", .label = "CRT curve", .section = section, .show = shown, .value = &crt->curvature, .choices = ph_crt_curvatures, .choice_count = SDL_arraysize(ph_crt_curvatures) };
    settings[4] = (struct ph_setting){ .key = "crt_glow", .label = "CRT glow", .section = section, .show = shown, .value = &crt->glow, .choices = ph_crt_glows, .choice_count = SDL_arraysize(ph_crt_glows) };
    return PH_CRT_SETTINGS;
}

static float ph_crt_line_darkness(int row, int rows, int source)
{
    float total = 0;
    for (int sample = 0; sample < PH_CRT_LINE_SAMPLES; sample++) {
        float line = (row + (sample + 0.5f) / PH_CRT_LINE_SAMPLES) * source / rows;
        float within = line - SDL_floorf(line);
        total += SDL_clamp((within - ph_crt_line_dark_from) / ph_crt_line_dark_ramp, 0.0f, 1.0f);
    }
    return total / PH_CRT_LINE_SAMPLES;
}

uint8_t ph_crt_scanline_alpha(int row, int rows, int source, int scanlines)
{
    int level = SDL_clamp(scanlines, 0, (int)SDL_arraysize(ph_crt_line_strengths) - 1);
    if (rows <= 0 || source <= 0 || level == 0) {
        return 0;
    }
    return (uint8_t)(ph_crt_line_darkness(row, rows, source) * ph_crt_line_strengths[level] * PH_CRT_ALPHA_MAX);
}

struct ph_crt_uniforms ph_crt_uniforms_for(const struct ph_crt *crt, float width, float height)
{
    int scanlines = SDL_clamp(crt->scanlines, 0, (int)SDL_arraysize(ph_crt_scan_hardness) - 1);
    int mask = SDL_clamp(crt->mask, 0, (int)SDL_arraysize(ph_crt_mask_types) - 1);
    int curvature = SDL_clamp(crt->curvature, 0, (int)SDL_arraysize(ph_crt_corners) - 1);
    int glow = SDL_clamp(crt->glow, 0, (int)SDL_arraysize(ph_crt_glow_levels) - 1);
    float safe_w = width > 0 ? width : 1.0f;
    float safe_h = height > 0 ? height : 1.0f;
    float scan = ph_crt_scan_hardness[scanlines];
    return (struct ph_crt_uniforms){
        .source = { safe_w, safe_h, 1.0f / safe_w, 1.0f / safe_h },
        .beam = { scan, scan * ph_crt_bright_scan_softening, ph_crt_pixel_hardness, ph_crt_glow_levels[glow] },
        .phosphors = { ph_crt_mask_types[mask], ph_crt_mask_dark, ph_crt_mask_light, ph_crt_brightness[scanlines] },
        .tube = { ph_crt_warps[curvature][0], ph_crt_warps[curvature][1], ph_crt_corners[curvature], 0.0f },
    };
}

static void ph_crt_prepare_lines(struct ph_crt *crt, int rows, int source)
{
    if (crt->lines && rows == crt->line_rows && source == crt->line_source && crt->scanlines == crt->line_level) {
        return;
    }
    SDL_DestroyTexture(crt->lines);
    crt->lines = SDL_CreateTexture(crt->renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, 1, rows);
    uint32_t *shades = SDL_malloc((size_t)rows * sizeof *shades);
    if (!crt->lines || !shades) {
        SDL_free(shades);
        return;
    }
    SDL_SetTextureBlendMode(crt->lines, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(crt->lines, SDL_SCALEMODE_NEAREST);
    for (int row = 0; row < rows; row++) {
        shades[row] = (uint32_t)ph_crt_scanline_alpha(row, rows, source, crt->scanlines) << PH_CRT_ALPHA_SHIFT;
    }
    SDL_UpdateTexture(crt->lines, NULL, shades, sizeof *shades);
    SDL_free(shades);
    crt->line_rows = rows;
    crt->line_source = source;
    crt->line_level = crt->scanlines;
}

static void ph_crt_draw_shader(struct ph_crt *crt, SDL_Texture *picture, SDL_FRect area)
{
    struct ph_crt_uniforms uniforms = ph_crt_uniforms_for(crt, (float)picture->w, (float)picture->h);
    SDL_SetGPURenderStateFragmentUniforms(crt->state, 0, &uniforms, sizeof uniforms);
    SDL_SetGPURenderState(crt->renderer, crt->state);
    SDL_RenderTexture(crt->renderer, picture, NULL, &area);
    SDL_SetGPURenderState(crt->renderer, NULL);
}

void ph_crt_draw(struct ph_crt *crt, SDL_Texture *picture, SDL_FRect area)
{
    SDL_SetTextureScaleMode(picture, ph_crt_scale_modes[crt->filter]);
    if (crt->filter == PH_FILTER_CRT && crt->state) {
        ph_crt_draw_shader(crt, picture, area);
        return;
    }
    SDL_RenderTexture(crt->renderer, picture, NULL, &area);
    if (crt->scanlines && area.h >= 1) {
        ph_crt_prepare_lines(crt, (int)area.h, picture->h);
        SDL_RenderTexture(crt->renderer, crt->lines, NULL, &area);
    }
}

#endif
