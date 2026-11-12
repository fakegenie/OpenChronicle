#version 450

layout(location = 0) in vec4 v_color;
layout(location = 1) in vec2 v_uv;
layout(location = 0) out vec4 o_color;

layout(set = 2, binding = 0) uniform sampler2D picture;

layout(set = 3, binding = 0) uniform Settings {
    vec4 source;
    vec4 beam;
    vec4 phosphors;
    vec4 tube;
};

vec3 to_linear(vec3 color)
{
    return pow(color, vec3(2.2));
}

vec3 to_display(vec3 color)
{
    return pow(color, vec3(1.0 / 2.2));
}

vec2 offset_from_texel(vec2 position)
{
    vec2 pixel = position * source.xy;
    return floor(pixel) + 0.5 - pixel;
}

vec3 fetch(vec2 position, vec2 offset)
{
    vec2 texel = (floor(position * source.xy + offset) + 0.5) * source.zw;
    return to_linear(texture(picture, texel).rgb);
}

float gaussian(float distance, float hardness)
{
    return exp2(hardness * distance * distance);
}

vec3 along_line(vec2 position, float line, int reach, float hardness)
{
    float offset = offset_from_texel(position).x;
    vec3 total = vec3(0.0);
    float weights = 0.0;
    for (int tap = -reach; tap <= reach; tap++) {
        float weight = gaussian(offset + float(tap), hardness);
        total += fetch(position, vec2(float(tap), line)) * weight;
        weights += weight;
    }
    return total / weights;
}

float beam_weight(vec2 position, float line, vec3 color)
{
    float brightness = sqrt(max(max(color.r, color.g), color.b));
    float hardness = mix(beam.x, beam.y, brightness);
    return gaussian(offset_from_texel(position).y + line, hardness);
}

vec3 beams(vec2 position)
{
    vec3 middle = along_line(position, 0.0, 2, beam.z);
    if (beam.x == 0.0) {
        return middle;
    }
    vec3 above = along_line(position, -1.0, 1, beam.z);
    vec3 below = along_line(position, 1.0, 1, beam.z);
    return above * beam_weight(position, -1.0, above) + middle * beam_weight(position, 0.0, middle) + below * beam_weight(position, 1.0, below);
}

vec3 glow(vec2 position)
{
    vec3 total = vec3(0.0);
    for (int line = -2; line <= 2; line++) {
        float weight = gaussian(offset_from_texel(position).y + float(line), -2.0);
        total += along_line(position, float(line), 3, -1.5) * weight;
    }
    return total;
}

vec3 phosphor_mask(vec2 pixel)
{
    vec3 mask = vec3(phosphors.y);
    float stripe = fract(pixel.x / 3.0);
    if (phosphors.x == 0.0) {
        return vec3(1.0);
    }
    if (phosphors.x == 3.0) {
        stripe = fract((pixel.x + floor(pixel.y) * 3.0) / 6.0);
    }
    if (stripe < 1.0 / 3.0) {
        mask.r = phosphors.z;
    } else if (stripe < 2.0 / 3.0) {
        mask.g = phosphors.z;
    } else {
        mask.b = phosphors.z;
    }
    float slot_row = fract((floor(pixel.y) + step(0.5, fract(pixel.x / 6.0)) * 2.0) / 4.0);
    if (phosphors.x == 2.0 && slot_row < 0.25) {
        mask *= phosphors.y;
    }
    return mask;
}

vec2 curve(vec2 uv)
{
    vec2 centered = uv * 2.0 - 1.0;
    centered *= vec2(1.0 + centered.y * centered.y * tube.x, 1.0 + centered.x * centered.x * tube.y);
    return centered * 0.5 + 0.5;
}

float screen_edge(vec2 position)
{
    vec2 inside = min(position, 1.0 - position) * source.xy;
    if (min(inside.x, inside.y) < 0.0) {
        return 0.0;
    }
    if (tube.z == 0.0) {
        return 1.0;
    }
    vec2 into_corner = max(vec2(tube.z) - inside, 0.0);
    return clamp(tube.z - length(into_corner), 0.0, 1.0);
}

void main()
{
    vec2 position = curve(v_uv);
    vec3 color = beams(position) * phosphor_mask(gl_FragCoord.xy) * phosphors.w;
    if (beam.w > 0.0) {
        color += glow(position) * beam.w;
    }
    o_color = vec4(to_display(color) * screen_edge(position), 1.0) * v_color;
}
