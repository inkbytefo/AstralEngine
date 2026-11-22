#version 460
#include <astral/scene.glsl>
layout(push_constant, scalar) uniform Push { Vertices vertices; Instances instances; FrameRef frame; Materials materials; uint first_instance; } pc;
layout(location = 0) out vec3 out_normal;
layout(location = 1) flat out uint out_instance;
layout(location = 2) out vec4 out_clip_now;  // unjittered, for motion vectors
layout(location = 3) out vec4 out_clip_prev;
layout(location = 4) out vec2 out_uv;
layout(location = 5) out vec3 out_world;
void main() {
    Vertex v = pc.vertices.data[gl_VertexIndex];
    uint id = pc.first_instance + gl_InstanceIndex;
    Instance inst = pc.instances.data[id];
    vec4 world = inst.model * vec4(v.position, 1.0);
    Frame f = pc.frame.f;
    gl_Position = f.view_projection * world;
    out_normal = normal_matrix_of(inst.model) * v.normal; // exact under non-uniform & negative scale
    out_instance = id;
    out_clip_now = f.unjittered_view_projection * world;
    out_clip_prev = f.prev_view_projection * (inst.prev_model * vec4(v.position, 1.0));
    out_uv = v.uv;
    out_world = world.xyz;
}
