#version 460
#include <astral/scene.glsl>
layout(push_constant, scalar) uniform Push { Vertices vertices; Instances instances; FrameRef frame; uint first_instance; } pc;
layout(location = 0) out vec3 out_normal;
layout(location = 1) flat out uint out_instance;
void main() {
    Vertex v = pc.vertices.data[gl_VertexIndex];
    uint id = pc.first_instance + gl_InstanceIndex;
    mat4 model = pc.instances.data[id].model;
    gl_Position = pc.frame.f.view_projection * (model * vec4(v.position, 1.0));
    out_normal = transpose(inverse(mat3(model))) * v.normal; // correct under non-uniform scale
    out_instance = id;
}
