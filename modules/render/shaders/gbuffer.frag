#version 460
#include <astral/scene.glsl>
layout(push_constant, scalar) uniform Push { Vertices vertices; Instances instances; FrameRef frame; uint first_instance; } pc;
layout(location = 0) in vec3 in_normal;
layout(location = 1) flat in uint in_instance;
layout(location = 0) out vec4 out_albedo;   // rgb albedo, a roughness
layout(location = 1) out vec4 out_normal;   // xyz world normal, w metallic
layout(location = 2) out vec4 out_emissive; // rgb emissive
void main() {
    Instance inst = pc.instances.data[in_instance];
    out_albedo = vec4(inst.albedo.rgb, inst.material.x);
    out_normal = vec4(normalize(in_normal), inst.material.y);
    out_emissive = vec4(inst.albedo.rgb * inst.material.z, 1.0);
}
