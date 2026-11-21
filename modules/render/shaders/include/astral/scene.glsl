// Shared scene data layouts. Mirrors the GPU structs in src/renderer.cpp (scalar layout).
#ifndef ASTRAL_SCENE_GLSL
#define ASTRAL_SCENE_GLSL
#include <astral/bindless.glsl>

struct Vertex { vec3 position; vec3 normal; vec2 uv; };
struct Instance {
    mat4 model;
    vec4 albedo;
    vec4 material; // x roughness, y metallic, z emissive strength
};
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
};
struct PointLight { vec4 position_radius; vec4 color_intensity; };
struct SdfShape {
    vec4 position_blend;   // xyz position, w smoothing radius
    vec4 rotation;         // quaternion (object -> world)
    vec4 size;             // primitive parameters
    vec4 albedo_roughness;
    vec4 params;           // x metallic, y operation, z primitive
};

ASTRAL_BUFFER(Vertices, Vertex);
ASTRAL_BUFFER(Instances, Instance);
ASTRAL_BUFFER(PointLights, PointLight);
ASTRAL_BUFFER(SdfShapes, SdfShape);
layout(buffer_reference, scalar, buffer_reference_align = 4) buffer FrameRef { Frame f; };

const float PI = 3.14159265359;

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
