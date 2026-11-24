#ifndef ASTRAL_SKY_IBL_GLSL
#define ASTRAL_SKY_IBL_GLSL
#include <astral/scene.glsl>

const float SKY_PI = 3.14159265359;
const uint SKY_SPECULAR_LEVELS = 6u;

vec2 sky_uv(vec3 direction) {
    float elevation = acos(clamp(direction.y, -1.0, 1.0));
    float azimuth = dot(direction.xz, direction.xz) > 1e-10 ? atan(direction.z, direction.x) : 0.0;
    return vec2(azimuth / (2.0 * SKY_PI) + 0.5, elevation / SKY_PI);
}

vec3 sky_direction(vec2 uv) {
    float phi = (uv.x - 0.5) * 2.0 * SKY_PI;
    float theta = uv.y * SKY_PI;
    return vec3(cos(phi) * sin(theta), cos(theta), sin(phi) * sin(theta));
}

vec3 sky_environment_sample(uint texture_id, vec3 direction) {
    vec2 uv = sky_uv(direction);
    float height = float(textureSize(sampler2D(astral_textures[texture_id],
                                               astral_samplers[ASTRAL_SAMPLER_LINEAR_REPEAT]), 0).y);
    uv.y = clamp(uv.y, 0.5 / height, 1.0 - 0.5 / height);
    return astral_sample(texture_id, ASTRAL_SAMPLER_LINEAR_REPEAT, uv).rgb;
}

vec3 sky_filtered_sample(uint texture_id, vec3 direction, uint layer) {
    vec2 uv = sky_uv(direction);
    float layers = float(SKY_SPECULAR_LEVELS + 1u);
    float height = float(textureSize(sampler2D(astral_textures[texture_id],
                                               astral_samplers[ASTRAL_SAMPLER_LINEAR_REPEAT]), 0).y) / layers;
    uv.y = (float(layer) + clamp(uv.y, 0.5 / height, 1.0 - 0.5 / height)) / layers;
    return astral_sample(texture_id, ASTRAL_SAMPLER_LINEAR_REPEAT, uv).rgb;
}
#endif
