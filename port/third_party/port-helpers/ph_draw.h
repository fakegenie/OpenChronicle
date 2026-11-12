#ifndef PH_DRAW_H
#define PH_DRAW_H

#include <SDL3/SDL.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ph_render_state {
    int logical_width;
    int logical_height;
    SDL_RendererLogicalPresentation mode;
    float scale_x;
    float scale_y;
    SDL_BlendMode blend;
    SDL_FColor color;
    int clipped;
    SDL_Rect clip;
    int viewported;
    SDL_Rect viewport;
};

struct ph_render_state ph_overlay_begin(SDL_Renderer *renderer);
void ph_overlay_end(SDL_Renderer *renderer, struct ph_render_state state);

SDL_FColor ph_color(uint32_t rgb, float alpha);
void ph_fill_rect(SDL_Renderer *renderer, SDL_FRect rect, SDL_FColor color);
void ph_fill_round(SDL_Renderer *renderer, SDL_FRect rect, float radius, SDL_FColor color);
void ph_stroke_round(SDL_Renderer *renderer, SDL_FRect rect, float radius, float width, SDL_FColor color);
void ph_fill_circle(SDL_Renderer *renderer, float x, float y, float radius, SDL_FColor color);
void ph_fill_triangle(SDL_Renderer *renderer, SDL_FPoint a, SDL_FPoint b, SDL_FPoint c, SDL_FColor color);
void ph_fill_fan(SDL_Renderer *renderer, SDL_FPoint center, const SDL_FPoint *points, int count, SDL_FColor color);
void ph_line(SDL_Renderer *renderer, SDL_FPoint from, SDL_FPoint to, float width, SDL_FColor color);
void ph_polyline(SDL_Renderer *renderer, const SDL_FPoint *points, int count, float width, SDL_FColor color);
SDL_Texture *ph_texture_png(SDL_Renderer *renderer, const void *data, size_t size);

enum {
    PH_TEXT_MEDIUM,
    PH_TEXT_BOLD,
};

float ph_text_width(const char *text, float size, int weight);
void ph_text(SDL_Renderer *renderer, const char *text, float x, float y, float size, int weight, SDL_FColor color);
void ph_text_centered(SDL_Renderer *renderer, const char *text, float center_x, float center_y, float size, int weight, SDL_FColor color);
void ph_text_fit(const char *text, float size, int weight, float width, char *out, int out_size);

#ifdef __cplusplus
}
#endif

#endif

#if defined(PH_IMPLEMENTATION) && !defined(PH_DRAW_IMPLEMENTED)
#define PH_DRAW_IMPLEMENTED

#include "ph_inter.h"

enum {
    PH_MESH_VERTICES = 4096,
    PH_MESH_INDICES = 12288,
    PH_CORNER_STEPS = 8,
    PH_OUTLINE_POINTS = 4 * (PH_CORNER_STEPS + 1),
    PH_FACES = 24,
    PH_FACE_SMALLEST = 6,
    PH_FACE_LARGEST = 256,
    PH_CURVE_STEPS = 32,
};

struct ph_face {
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    int weight;
    int pixels;
    int ascent;
    SDL_Rect boxes[PH_INTER_GLYPHS];
    SDL_Point offsets[PH_INTER_GLYPHS];
};

struct ph_raster {
    float *cells;
    int width;
    int height;
    SDL_FPoint pen;
};

static struct ph_face ph_faces[PH_FACES];
static int ph_next_face;

static SDL_Vertex ph_mesh_vertices[PH_MESH_VERTICES];
static int ph_mesh_indices[PH_MESH_INDICES];
static int ph_mesh_vertex_count;
static int ph_mesh_index_count;
static SDL_Renderer *ph_mesh_renderer;

static void ph_mesh_flush(void)
{
    if (ph_mesh_index_count) {
        SDL_RenderGeometry(ph_mesh_renderer, NULL, ph_mesh_vertices, ph_mesh_vertex_count, ph_mesh_indices, ph_mesh_index_count);
    }
    ph_mesh_vertex_count = 0;
    ph_mesh_index_count = 0;
}

static void ph_mesh_room(SDL_Renderer *renderer, int vertices, int indices)
{
    if (renderer != ph_mesh_renderer) {
        ph_mesh_flush();
        ph_mesh_renderer = renderer;
    }
    if (ph_mesh_vertex_count + vertices > PH_MESH_VERTICES || ph_mesh_index_count + indices > PH_MESH_INDICES) {
        ph_mesh_flush();
    }
}

static int ph_mesh_vertex(float x, float y, SDL_FColor color)
{
    ph_mesh_vertices[ph_mesh_vertex_count] = (SDL_Vertex){ { x, y }, color, { 0, 0 } };
    return ph_mesh_vertex_count++;
}

static void ph_mesh_triangle(int a, int b, int c)
{
    ph_mesh_indices[ph_mesh_index_count++] = a;
    ph_mesh_indices[ph_mesh_index_count++] = b;
    ph_mesh_indices[ph_mesh_index_count++] = c;
}

static SDL_FColor ph_clear(SDL_FColor color)
{
    return (SDL_FColor){ color.r, color.g, color.b, 0 };
}

struct ph_render_state ph_overlay_begin(SDL_Renderer *renderer)
{
    struct ph_render_state state;
    SDL_zero(state);
    SDL_GetRenderLogicalPresentation(renderer, &state.logical_width, &state.logical_height, &state.mode);
    SDL_GetRenderScale(renderer, &state.scale_x, &state.scale_y);
    SDL_GetRenderDrawBlendMode(renderer, &state.blend);
    SDL_GetRenderDrawColorFloat(renderer, &state.color.r, &state.color.g, &state.color.b, &state.color.a);
    state.clipped = SDL_RenderClipEnabled(renderer);
    SDL_GetRenderClipRect(renderer, &state.clip);
    state.viewported = SDL_RenderViewportSet(renderer);
    SDL_GetRenderViewport(renderer, &state.viewport);
    SDL_SetRenderLogicalPresentation(renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);
    SDL_SetRenderScale(renderer, 1, 1);
    SDL_SetRenderViewport(renderer, NULL);
    SDL_SetRenderClipRect(renderer, NULL);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    return state;
}

void ph_overlay_end(SDL_Renderer *renderer, struct ph_render_state state)
{
    ph_mesh_flush();
    SDL_SetRenderLogicalPresentation(renderer, state.logical_width, state.logical_height, state.mode);
    SDL_SetRenderScale(renderer, state.scale_x, state.scale_y);
    SDL_SetRenderViewport(renderer, state.viewported ? &state.viewport : NULL);
    SDL_SetRenderClipRect(renderer, state.clipped ? &state.clip : NULL);
    SDL_SetRenderDrawBlendMode(renderer, state.blend);
    SDL_SetRenderDrawColorFloat(renderer, state.color.r, state.color.g, state.color.b, state.color.a);
}

SDL_FColor ph_color(uint32_t rgb, float alpha)
{
    return (SDL_FColor){ (rgb >> 16 & 0xFF) / 255.0f, (rgb >> 8 & 0xFF) / 255.0f, (rgb & 0xFF) / 255.0f, alpha };
}

void ph_fill_rect(SDL_Renderer *renderer, SDL_FRect rect, SDL_FColor color)
{
    ph_mesh_room(renderer, 4, 6);
    int a = ph_mesh_vertex(rect.x, rect.y, color);
    int b = ph_mesh_vertex(rect.x + rect.w, rect.y, color);
    int c = ph_mesh_vertex(rect.x + rect.w, rect.y + rect.h, color);
    int d = ph_mesh_vertex(rect.x, rect.y + rect.h, color);
    ph_mesh_triangle(a, b, c);
    ph_mesh_triangle(a, c, d);
    ph_mesh_flush();
}

static void ph_round_outline(SDL_FRect rect, float radius, SDL_FPoint *points, SDL_FPoint *normals)
{
    float limit = SDL_min(rect.w, rect.h) / 2;
    radius = SDL_clamp(radius, 0, limit);
    SDL_FPoint centers[4] = {
        { rect.x + radius, rect.y + radius },
        { rect.x + rect.w - radius, rect.y + radius },
        { rect.x + rect.w - radius, rect.y + rect.h - radius },
        { rect.x + radius, rect.y + rect.h - radius },
    };
    for (int corner = 0; corner < 4; corner++) {
        for (int step = 0; step <= PH_CORNER_STEPS; step++) {
            float angle = SDL_PI_F + corner * SDL_PI_F / 2 + step * SDL_PI_F / 2 / PH_CORNER_STEPS;
            int index = corner * (PH_CORNER_STEPS + 1) + step;
            normals[index] = (SDL_FPoint){ SDL_cosf(angle), SDL_sinf(angle) };
            points[index] = (SDL_FPoint){ centers[corner].x + normals[index].x * radius, centers[corner].y + normals[index].y * radius };
        }
    }
}

void ph_fill_round(SDL_Renderer *renderer, SDL_FRect rect, float radius, SDL_FColor color)
{
    SDL_FPoint points[PH_OUTLINE_POINTS];
    SDL_FPoint normals[PH_OUTLINE_POINTS];
    ph_round_outline(rect, radius, points, normals);
    ph_mesh_room(renderer, 1 + PH_OUTLINE_POINTS * 2, PH_OUTLINE_POINTS * 9);
    int center = ph_mesh_vertex(rect.x + rect.w / 2, rect.y + rect.h / 2, color);
    for (int index = 0; index < PH_OUTLINE_POINTS; index++) {
        ph_mesh_vertex(points[index].x - normals[index].x * 0.5f, points[index].y - normals[index].y * 0.5f, color);
        ph_mesh_vertex(points[index].x + normals[index].x * 0.5f, points[index].y + normals[index].y * 0.5f, ph_clear(color));
    }
    for (int index = 0; index < PH_OUTLINE_POINTS; index++) {
        int inner = center + 1 + index * 2;
        int next = center + 1 + (index + 1) % PH_OUTLINE_POINTS * 2;
        ph_mesh_triangle(center, inner, next);
        ph_mesh_triangle(inner, inner + 1, next);
        ph_mesh_triangle(next, inner + 1, next + 1);
    }
    ph_mesh_flush();
}

void ph_stroke_round(SDL_Renderer *renderer, SDL_FRect rect, float radius, float width, SDL_FColor color)
{
    SDL_FPoint points[PH_OUTLINE_POINTS];
    SDL_FPoint normals[PH_OUTLINE_POINTS];
    ph_round_outline(rect, radius, points, normals);
    width = SDL_max(width, 1);
    float offsets[4] = { -width / 2 - 0.5f, -width / 2 + 0.5f, width / 2 - 0.5f, width / 2 + 0.5f };
    ph_mesh_room(renderer, PH_OUTLINE_POINTS * 4, PH_OUTLINE_POINTS * 18);
    int first = ph_mesh_vertex_count;
    for (int index = 0; index < PH_OUTLINE_POINTS; index++) {
        for (int band = 0; band < 4; band++) {
            SDL_FColor ink = band == 0 || band == 3 ? ph_clear(color) : color;
            ph_mesh_vertex(points[index].x + normals[index].x * offsets[band], points[index].y + normals[index].y * offsets[band], ink);
        }
    }
    for (int index = 0; index < PH_OUTLINE_POINTS; index++) {
        for (int band = 0; band < 3; band++) {
            int a = first + index * 4 + band;
            int b = first + (index + 1) % PH_OUTLINE_POINTS * 4 + band;
            ph_mesh_triangle(a, a + 1, b);
            ph_mesh_triangle(b, a + 1, b + 1);
        }
    }
    ph_mesh_flush();
}

void ph_fill_circle(SDL_Renderer *renderer, float x, float y, float radius, SDL_FColor color)
{
    ph_fill_round(renderer, (SDL_FRect){ x - radius, y - radius, radius * 2, radius * 2 }, radius, color);
}

void ph_fill_triangle(SDL_Renderer *renderer, SDL_FPoint a, SDL_FPoint b, SDL_FPoint c, SDL_FColor color)
{
    ph_mesh_room(renderer, 3, 3);
    int first = ph_mesh_vertex(a.x, a.y, color);
    ph_mesh_vertex(b.x, b.y, color);
    ph_mesh_vertex(c.x, c.y, color);
    ph_mesh_triangle(first, first + 1, first + 2);
    ph_mesh_flush();
}

void ph_fill_fan(SDL_Renderer *renderer, SDL_FPoint center, const SDL_FPoint *points, int count, SDL_FColor color)
{
    if (count < 2 || count + 1 > PH_MESH_VERTICES || count * 3 > PH_MESH_INDICES) {
        return;
    }
    ph_mesh_room(renderer, count + 1, count * 3);
    int middle = ph_mesh_vertex(center.x, center.y, color);
    for (int index = 0; index < count; index++) {
        ph_mesh_vertex(points[index].x, points[index].y, color);
    }
    for (int index = 0; index < count; index++) {
        ph_mesh_triangle(middle, middle + 1 + index, middle + 1 + (index + 1) % count);
    }
    ph_mesh_flush();
}

static void ph_mesh_joint(float x, float y, float radius, SDL_FColor color)
{
    int steps = radius < 1.5f ? 4 : 8;
    int center = ph_mesh_vertex(x, y, color);
    for (int step = 0; step < steps; step++) {
        float angle = step * 2 * SDL_PI_F / steps;
        ph_mesh_vertex(x + SDL_cosf(angle) * radius, y + SDL_sinf(angle) * radius, color);
    }
    for (int step = 0; step < steps; step++) {
        ph_mesh_triangle(center, center + 1 + step, center + 1 + (step + 1) % steps);
    }
}

static void ph_mesh_segment(SDL_FPoint from, SDL_FPoint to, float width, SDL_FColor color)
{
    float length = SDL_sqrtf((to.x - from.x) * (to.x - from.x) + (to.y - from.y) * (to.y - from.y));
    if (length <= 0) {
        return;
    }
    float dx = (to.y - from.y) / length * width / 2;
    float dy = (from.x - to.x) / length * width / 2;
    int first = ph_mesh_vertex(from.x + dx, from.y + dy, color);
    ph_mesh_vertex(to.x + dx, to.y + dy, color);
    ph_mesh_vertex(to.x - dx, to.y - dy, color);
    ph_mesh_vertex(from.x - dx, from.y - dy, color);
    ph_mesh_triangle(first, first + 1, first + 2);
    ph_mesh_triangle(first, first + 2, first + 3);
}

void ph_line(SDL_Renderer *renderer, SDL_FPoint from, SDL_FPoint to, float width, SDL_FColor color)
{
    ph_mesh_room(renderer, 4, 6);
    ph_mesh_segment(from, to, width, color);
    ph_mesh_flush();
}

void ph_polyline(SDL_Renderer *renderer, const SDL_FPoint *points, int count, float width, SDL_FColor color)
{
    for (int index = 0; index < count; index++) {
        ph_mesh_room(renderer, 13, 30);
        ph_mesh_joint(points[index].x, points[index].y, width / 2, color);
        if (index > 0) {
            ph_mesh_segment(points[index - 1], points[index], width, color);
        }
    }
    ph_mesh_flush();
}

SDL_Texture *ph_texture_png(SDL_Renderer *renderer, const void *data, size_t size)
{
    SDL_Surface *surface = SDL_LoadPNG_IO(SDL_IOFromConstMem(data, size), true);
    SDL_Texture *texture = surface ? SDL_CreateTextureFromSurface(renderer, surface) : NULL;
    SDL_DestroySurface(surface);
    return texture;
}

static int ph_glyph_index(Uint32 codepoint)
{
    int fallback = 0;
    for (int index = 0; index < PH_INTER_GLYPHS; index++) {
        int found = ph_inter_fonts[0].glyphs[index].codepoint;
        if (found == (int)codepoint) {
            return index;
        }
        if (found == '?') {
            fallback = index;
        }
    }
    return fallback;
}

static int ph_font_kern(const struct ph_font *font, int first, int second)
{
    int pair = first << 8 | second;
    int low = 0;
    int high = font->kern_count - 1;
    while (low <= high) {
        int middle = (low + high) / 2;
        if (font->kerns[middle].pair == pair) {
            return font->kerns[middle].amount;
        }
        if (font->kerns[middle].pair < pair) {
            low = middle + 1;
        } else {
            high = middle - 1;
        }
    }
    return 0;
}

static const struct ph_font *ph_font_weight(int weight)
{
    return &ph_inter_fonts[weight == PH_TEXT_BOLD ? PH_TEXT_BOLD : PH_TEXT_MEDIUM];
}

static int ph_face_pixels(float size)
{
    int pixels = (int)SDL_roundf(size);
    return SDL_clamp(pixels, PH_FACE_SMALLEST, PH_FACE_LARGEST);
}

static float ph_font_scale(const struct ph_font *font, int pixels)
{
    return (float)pixels / (float)(font->ascent - font->descent);
}

static float ph_glyph_advance(const struct ph_font *font, int glyph, Uint32 next, float scale)
{
    int kern = next ? ph_font_kern(font, glyph, ph_glyph_index(next)) : 0;
    float adv = (float)(font->glyphs[glyph].advance + kern) * scale;
    if (scale * (float)(font->ascent - font->descent) <= 16.5f && adv > 0.0f) {
        return SDL_max(1.0f, SDL_roundf(adv));
    }
    return adv;
}

static void ph_raster_line(struct ph_raster *raster, SDL_FPoint to)
{
    SDL_FPoint from = raster->pen;
    raster->pen = to;
    if (from.y == to.y) {
        return;
    }
    float direction = from.y < to.y ? 1.0f : -1.0f;
    SDL_FPoint top = from.y < to.y ? from : to;
    SDL_FPoint bottom = from.y < to.y ? to : from;
    float slope = (bottom.x - top.x) / (bottom.y - top.y);
    float x = top.x;
    int end_row = (int)SDL_ceilf(bottom.y);
    end_row = SDL_min(end_row, raster->height);
    for (int row = (int)top.y; row < end_row; row++) {
        float *line = raster->cells + row * raster->width;
        float rise = SDL_min(row + 1.0f, bottom.y) - SDL_max((float)row, top.y);
        float next = x + slope * rise;
        float amount = rise * direction;
        float left = SDL_min(x, next);
        float right = SDL_max(x, next);
        float left_floor = SDL_floorf(left);
        float right_ceil = SDL_ceilf(right);
        int first = (int)left_floor;
        int last = (int)right_ceil;
        if (last <= first + 1) {
            float middle = 0.5f * (x + next) - left_floor;
            line[first] += amount - amount * middle;
            line[first + 1] += amount * middle;
        } else {
            float inverse = 1 / (right - left);
            float left_part = left - left_floor;
            float right_part = right - right_ceil + 1;
            float opening = 0.5f * inverse * (1 - left_part) * (1 - left_part);
            float closing = 0.5f * inverse * right_part * right_part;
            line[first] += amount * opening;
            if (last == first + 2) {
                line[first + 1] += amount * (1 - opening - closing);
            } else {
                float second = inverse * (1.5f - left_part);
                line[first + 1] += amount * (second - opening);
                for (int column = first + 2; column < last - 1; column++) {
                    line[column] += amount * inverse;
                }
                float before_last = second + (float)(last - first - 3) * inverse;
                line[last - 1] += amount * (1 - before_last - closing);
            }
            line[last] += amount * closing;
        }
        x = next;
    }
}

static void ph_raster_curve(struct ph_raster *raster, SDL_FPoint control, SDL_FPoint to)
{
    SDL_FPoint from = raster->pen;
    float bend_x = from.x - 2 * control.x + to.x;
    float bend_y = from.y - 2 * control.y + to.y;
    int steps = (int)SDL_ceilf(SDL_sqrtf(SDL_sqrtf(bend_x * bend_x + bend_y * bend_y) * 1.25f));
    steps = SDL_clamp(steps, 1, PH_CURVE_STEPS);
    for (int step = 1; step <= steps; step++) {
        float t = (float)step / (float)steps;
        float u = 1 - t;
        SDL_FPoint point = { u * u * from.x + 2 * u * t * control.x + t * t * to.x, u * u * from.y + 2 * u * t * control.y + t * t * to.y };
        ph_raster_line(raster, point);
    }
}

static SDL_FPoint ph_outline_point(const short *point, float scale, SDL_Point offset)
{
    return (SDL_FPoint){ point[0] * scale - (float)offset.x, -point[1] * scale - (float)offset.y };
}

static SDL_FPoint ph_midpoint(SDL_FPoint a, SDL_FPoint b)
{
    return (SDL_FPoint){ (a.x + b.x) / 2, (a.y + b.y) / 2 };
}

static const short *ph_raster_contour(struct ph_raster *raster, const short *contour, float scale, SDL_Point offset)
{
    int count = contour[0];
    const short *points = contour + 1;
    const short *final = points + (count - 1) * 3;
    int first = 0;
    int visits = count;
    SDL_FPoint start;
    if (points[2]) {
        start = ph_outline_point(points, scale, offset);
        first = 1;
        visits = count - 1;
    } else if (final[2]) {
        start = ph_outline_point(final, scale, offset);
        visits = count - 1;
    } else {
        start = ph_midpoint(ph_outline_point(final, scale, offset), ph_outline_point(points, scale, offset));
    }
    raster->pen = start;
    SDL_FPoint control = start;
    int curved = 0;
    for (int visit = 0; visit < visits; visit++) {
        const short *point = points + (first + visit) * 3;
        SDL_FPoint here = ph_outline_point(point, scale, offset);
        if (point[2] && curved) {
            ph_raster_curve(raster, control, here);
        } else if (point[2]) {
            ph_raster_line(raster, here);
        } else if (curved) {
            ph_raster_curve(raster, control, ph_midpoint(control, here));
        }
        control = here;
        curved = !point[2];
    }
    if (curved) {
        ph_raster_curve(raster, control, start);
    } else {
        ph_raster_line(raster, start);
    }
    return points + count * 3;
}

static void ph_face_glyph(struct ph_face *face, SDL_Surface *surface, const struct ph_font *font, int glyph, float scale)
{
    SDL_Rect box = face->boxes[glyph];
    struct ph_raster raster = { SDL_calloc((size_t)(box.w * box.h + 4), sizeof(float)), box.w, box.h, { 0, 0 } };
    if (!raster.cells) {
        return;
    }
    const short *shape = font->shapes + font->glyphs[glyph].shape;
    int contours = shape[0];
    shape++;
    for (int contour = 0; contour < contours; contour++) {
        shape = ph_raster_contour(&raster, shape, scale, face->offsets[glyph]);
    }
    float sum = 0;
    for (int cell = 0; cell < box.w * box.h; cell++) {
        sum += raster.cells[cell];
        float coverage = SDL_fabsf(sum);
        if (face->pixels <= 16) {
            coverage = coverage >= 0.48f ? 1.0f : 0.0f;
        } else if (face->pixels <= 25) {
            coverage = SDL_clamp((coverage - 0.30f) / 0.40f, 0.0f, 1.0f);
        }
        Uint8 *pixel = (Uint8 *)surface->pixels + (box.y + cell / box.w) * surface->pitch + (box.x + cell % box.w) * 4;
        pixel[3] = (Uint8)(SDL_min(coverage, 1.0f) * 255 + 0.5f);
    }
    SDL_free(raster.cells);
}

static int ph_face_pack(struct ph_face *face, int side)
{
    int x = 0;
    int y = 0;
    int shelf = 0;
    for (int glyph = 0; glyph < PH_INTER_GLYPHS; glyph++) {
        SDL_Rect *box = &face->boxes[glyph];
        if (x + box->w > side) {
            x = 0;
            y += shelf;
            shelf = 0;
        }
        if (box->w > side || y + box->h > side) {
            return 0;
        }
        box->x = x;
        box->y = y;
        x += box->w;
        shelf = SDL_max(shelf, box->h);
    }
    return 1;
}

static void ph_face_build(struct ph_face *face, SDL_Renderer *renderer, int weight, int pixels)
{
    const struct ph_font *font = &ph_inter_fonts[weight];
    float scale = ph_font_scale(font, pixels);
    face->renderer = renderer;
    face->texture = NULL;
    face->weight = weight;
    face->pixels = pixels;
    face->ascent = (int)SDL_roundf(font->ascent * scale);
    for (int glyph = 0; glyph < PH_INTER_GLYPHS; glyph++) {
        const struct ph_font_glyph *info = &font->glyphs[glyph];
        int left = (int)SDL_floorf(info->left * scale);
        int top = (int)SDL_floorf(-info->top * scale);
        int right = (int)SDL_ceilf(info->right * scale);
        int bottom = (int)SDL_ceilf(-info->bottom * scale);
        face->offsets[glyph] = (SDL_Point){ left - 1, top - 1 };
        face->boxes[glyph] = (SDL_Rect){ 0, 0, right - left + 2, bottom - top + 2 };
    }
    int side = 64;
    while (!ph_face_pack(face, side)) {
        side *= 2;
    }
    SDL_Surface *surface = SDL_CreateSurface(side, side, SDL_PIXELFORMAT_RGBA32);
    if (!surface) {
        return;
    }
    SDL_FillSurfaceRect(surface, NULL, SDL_MapSurfaceRGBA(surface, 255, 255, 255, 0));
    for (int glyph = 0; glyph < PH_INTER_GLYPHS; glyph++) {
        ph_face_glyph(face, surface, font, glyph, scale);
    }
    face->texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_DestroySurface(surface);
    SDL_SetTextureScaleMode(face->texture, SDL_SCALEMODE_NEAREST);
    SDL_SetTextureBlendMode(face->texture, SDL_BLENDMODE_BLEND);
}

static struct ph_face *ph_face_for(SDL_Renderer *renderer, float size, int weight)
{
    int pixels = ph_face_pixels(size);
    int bold = weight == PH_TEXT_BOLD;
    for (int index = 0; index < PH_FACES; index++) {
        struct ph_face *face = &ph_faces[index];
        if (face->texture && face->renderer == renderer && face->weight == bold && face->pixels == pixels && SDL_GetRendererFromTexture(face->texture) == renderer) {
            return face;
        }
    }
    struct ph_face *face = &ph_faces[ph_next_face];
    ph_next_face = (ph_next_face + 1) % PH_FACES;
    if (face->texture) {
        SDL_DestroyTexture(face->texture);
    }
    ph_face_build(face, renderer, bold, pixels);
    return face;
}

float ph_text_width(const char *text, float size, int weight)
{
    const struct ph_font *font = ph_font_weight(weight);
    float scale = ph_font_scale(font, ph_face_pixels(size));
    float width = 0;
    const char *cursor = text;
    Uint32 codepoint = SDL_StepUTF8(&cursor, NULL);
    while (codepoint) {
        Uint32 next = SDL_StepUTF8(&cursor, NULL);
        width += ph_glyph_advance(font, ph_glyph_index(codepoint), next, scale);
        codepoint = next;
    }
    return width;
}

void ph_text(SDL_Renderer *renderer, const char *text, float x, float y, float size, int weight, SDL_FColor color)
{
    struct ph_face *face = ph_face_for(renderer, size, weight);
    if (!face->texture) {
        return;
    }
    const struct ph_font *font = &ph_inter_fonts[face->weight];
    float scale = ph_font_scale(font, face->pixels);
    float baseline = SDL_roundf(y) + (float)face->ascent;
    float pen = x;
    SDL_SetTextureColorModFloat(face->texture, color.r, color.g, color.b);
    SDL_SetTextureAlphaModFloat(face->texture, color.a);
    const char *cursor = text;
    Uint32 codepoint = SDL_StepUTF8(&cursor, NULL);
    while (codepoint) {
        Uint32 next = SDL_StepUTF8(&cursor, NULL);
        int glyph = ph_glyph_index(codepoint);
        SDL_Rect box = face->boxes[glyph];
        SDL_FRect source = { (float)box.x, (float)box.y, (float)box.w, (float)box.h };
        SDL_FRect target = { SDL_roundf(pen) + (float)face->offsets[glyph].x, baseline + (float)face->offsets[glyph].y, source.w, source.h };
        SDL_RenderTexture(renderer, face->texture, &source, &target);
        pen += ph_glyph_advance(font, glyph, next, scale);
        codepoint = next;
    }
}

void ph_text_centered(SDL_Renderer *renderer, const char *text, float center_x, float center_y, float size, int weight, SDL_FColor color)
{
    ph_text(renderer, text, center_x - ph_text_width(text, size, weight) / 2, center_y - size / 2, size, weight, color);
}

void ph_text_fit(const char *text, float size, int weight, float width, char *out, int out_size)
{
    const char *ellipsis = "\xE2\x80\xA6";
    SDL_strlcpy(out, text, (size_t)out_size);
    if (ph_text_width(out, size, weight) <= width) {
        return;
    }
    float dots = ph_text_width(ellipsis, size, weight);
    int length = (int)SDL_strlen(out);
    while (length > 0 && (length + 4 > out_size || ph_text_width(out, size, weight) + dots > width)) {
        length--;
        while (length > 0 && ((unsigned char)out[length] & 0xC0) == 0x80) {
            length--;
        }
        out[length] = 0;
    }
    while (length > 0 && out[length - 1] == ' ') {
        out[--length] = 0;
    }
    SDL_strlcat(out, ellipsis, (size_t)out_size);
}

#endif
