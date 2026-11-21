#version 460
#include <astral/scene.glsl>
layout(push_constant, scalar) uniform Push { Vertices vertices; Instances instances; FrameRef frame; Materials materials; uint first_instance; } pc;
layout(location = 0) in vec3 in_normal;
layout(location = 1) flat in uint in_instance;
layout(location = 2) in vec4 in_clip_now;
layout(location = 3) in vec4 in_clip_prev;
layout(location = 0) out vec4 out_albedo;   // rgb albedo, a roughness
layout(location = 1) out vec4 out_normal;   // xyz world normal, w metallic
layout(location = 2) out vec4 out_emissive; // rgb emissive
layout(location = 3) out vec2 out_velocity; // screen uv now - uv last frame
void main() {
    Material m = pc.materials.data[pc.instances.data[in_instance].material];
    out_albedo = vec4(m.base_color.rgb, m.emissive_roughness.a);
    out_normal = vec4(normalize(in_normal), m.params.x);
    out_emissive = vec4(m.emissive_roughness.rgb, 1.0);
    vec2 now = in_clip_now.xy / in_clip_now.w * 0.5 + 0.5;
    vec2 prev = in_clip_prev.xy / max(in_clip_prev.w, 1e-6) * 0.5 + 0.5;
    out_velocity = now - prev;
}
