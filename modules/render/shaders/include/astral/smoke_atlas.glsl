#ifndef ASTRAL_SMOKE_ATLAS_GLSL
#define ASTRAL_SMOKE_ATLAS_GLSL
#include <astral/scene.glsl>

const uint SMOKE_BRICK_EDGE = 8u;
const uint SMOKE_ATLAS_COLUMNS = 16u;
ASTRAL_BUFFER(SmokeWords, uint);

uint smoke_brick(SmokeWords words, uint first, uint side, uvec3 cell) {
    uint grid = (side + SMOKE_BRICK_EDGE - 1u) / SMOKE_BRICK_EDGE;
    uvec3 page = cell / SMOKE_BRICK_EDGE;
    return words.data[first + (page.z * grid + page.y) * grid + page.x];
}

ivec2 smoke_atlas_texel(uint brick, uvec3 cell, uint planes) {
    uint edge = SMOKE_BRICK_EDGE;
    uvec3 local = cell % edge;
    return ivec2((brick % SMOKE_ATLAS_COLUMNS) * edge + local.x,
                 (brick / SMOKE_ATLAS_COLUMNS) * edge * edge * planes + local.z * edge + local.y);
}
#endif
