// Shared colour-space, luminance, and tonemapping utilities.
// Single source of truth across raster, ray-traced, custom .ashader, and UI passes.
#ifndef ASTRAL_COLOR_GLSL
#define ASTRAL_COLOR_GLSL

float astral_luminance(vec3 c) {
    return dot(c, vec3(0.2126, 0.7152, 0.0722));
}

// Exact IEC 61966-2-1 sRGB transfer functions (matches hardware sRGB attachments and CPU mip builder).
vec3 astral_srgb_to_linear(vec3 c) {
    c = max(c, vec3(0.0));
    return mix(c / 12.92, pow((c + 0.055) / 1.055, vec3(2.4)), step(0.04045, c));
}

vec3 astral_linear_to_srgb(vec3 c) {
    c = clamp(c, vec3(0.0), vec3(1.0));
    return mix(c * 12.92, 1.055 * pow(c, vec3(1.0 / 2.4)) - 0.055, step(0.0031308, c));
}

// Narkowicz (2015) ACES filmic tonemapping curve fit.
vec3 astral_aces_tonemap(vec3 x) {
    x = max(x, vec3(0.0));
    return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}

#endif
