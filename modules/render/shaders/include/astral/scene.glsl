// Shared scene data layouts. Mirrors the GPU structs in src/renderer.cpp (scalar layout).
#ifndef ASTRAL_SCENE_GLSL
#define ASTRAL_SCENE_GLSL
#include <astral/bindless.glsl>

struct Vertex { vec3 position; vec3 normal; vec2 uv; };
struct Instance {
    mat4 model;
    mat4 prev_model;   // last frame's transform (motion vectors)
    uint material;     // index into the material table
    uvec2 vertices;    // Vertices address as 2 x uint32 (keeps the struct 4-byte aligned)
    uvec2 indices;     // uint32 triangle indices address
};
struct Material {
    vec4 base_color;
    vec4 emissive_roughness; // rgb emitted radiance, a roughness
    vec4 params;             // x metallic, y normal scale
    uvec4 textures;          // bindless: base colour, metallic-roughness (G rough, B metal), normal, emissive; ~0u none
    uvec4 custom;            // x: .ashader parameter offset (words), y: custom shader index + 1 (0 = standard), zw: unused
};
const uint NO_TEXTURE = 0xFFFFFFFFu;
struct Frame {
    mat4 view_projection;
    mat4 inv_view_projection;
    mat4 light_view_projection;
    vec4 camera_position;
    vec4 sun_direction;   // xyz: direction of travel
    vec4 sun_color;       // rgb * intensity
    vec4 sky_color;       // rgb, a: ambient strength
    vec4 ground_color;    // rgb, a: exposure
    uvec4 counts;         // x: point lights, y: sdf shapes, z: shadow map bindless index (or ~0u), w: unused
    vec4 screen;          // width, height, 1/width, 1/height
    // --- ray-traced path ---
    mat4 prev_view_projection; // previous frame, unjittered (reprojection)
    vec4 ddgi_origin;     // xyz first probe position, w hysteresis
    vec4 ddgi_spacing;    // xyz probe spacing, w unused
    uvec4 ddgi_counts;    // xyz probes per axis, w rays per probe
    uvec4 rt_counts;      // x rect lights, y frame index, z history valid, w ddgi history valid
    vec4 rt_params;       // x sun cone half-angle (rad), yz jitter (pixels), w max ray distance
    mat4 unjittered_view_projection; // this frame without TAA jitter (motion vectors)
    uvec4 custom;         // xy: .ashader parameter block address, z: TIME (float bits)
};
struct PointLight { vec4 position_radius; vec4 color_intensity; vec4 source; /* x source radius */ };
struct SdfShape {
    vec4 position_blend;   // xyz position, w smoothing radius
    vec4 rotation;         // quaternion (object -> world)
    vec4 size;             // primitive parameters
    vec4 albedo_roughness;
    vec4 params;           // x metallic, y operation, z primitive
};

ASTRAL_BUFFER(Vertices, Vertex);
ASTRAL_BUFFER(Instances, Instance);
ASTRAL_BUFFER(Materials, Material);
ASTRAL_BUFFER(PointLights, PointLight);
ASTRAL_BUFFER(SdfShapes, SdfShape);
layout(buffer_reference, scalar, buffer_reference_align = 4) buffer FrameRef { Frame f; };

const float PI = 3.14159265359;

uint64_t address_of(uvec2 a) { return uint64_t(a.x) | (uint64_t(a.y) << 32); }

vec3 quat_rotate(vec4 q, vec3 v) {
    vec3 t = 2.0 * cross(q.xyz, v);
    return v + q.w * t + cross(q.xyz, t);
}

vec3 reconstruct_world(Frame f, vec2 uv, float depth) {
    vec4 ndc = vec4(uv * 2.0 - 1.0, depth, 1.0);
    vec4 world = f.inv_view_projection * ndc;
    return world.xyz / world.w;
}

#endif
