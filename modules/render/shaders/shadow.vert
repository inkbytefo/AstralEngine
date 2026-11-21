#version 460
#include <astral/scene.glsl>
layout(push_constant, scalar) uniform Push { Vertices vertices; Instances instances; FrameRef frame; Materials materials; uint first_instance; } pc;
void main() {
    Vertex v = pc.vertices.data[gl_VertexIndex];
    mat4 model = pc.instances.data[pc.first_instance + gl_InstanceIndex].model;
    gl_Position = pc.frame.f.light_view_projection * (model * vec4(v.position, 1.0));
}
