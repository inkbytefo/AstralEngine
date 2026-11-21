#version 460
#include <astral/bindless.glsl>

struct Vertex { vec3 position; vec3 normal; vec2 uv; };
struct Instance { mat4 model; vec4 color; };
struct Frame {
    mat4 view_projection;
    vec4 camera_position;
    vec4 light_direction; // xyz: direction of travel
    vec4 light_color;     // rgb * intensity, a: ambient
};
ASTRAL_BUFFER(Vertices, Vertex);
ASTRAL_BUFFER(Instances, Instance);
layout(buffer_reference, scalar, buffer_reference_align = 4) buffer FrameRef { Frame frame; };

layout(push_constant, scalar) uniform Push {
    Vertices vertices;
    Instances instances;
    FrameRef frame;
    uint first_instance;
} pc;

layout(location = 0) out vec3 out_normal;
layout(location = 1) out vec4 out_color;
layout(location = 2) out vec3 out_world;

void main() {
    Vertex v = pc.vertices.data[gl_VertexIndex];
    Instance inst = pc.instances.data[pc.first_instance + gl_InstanceIndex];
    vec4 world = inst.model * vec4(v.position, 1.0);
    gl_Position = pc.frame.frame.view_projection * world;
    out_normal = mat3(inst.model) * v.normal; // fine for uniform scale; non-uniform later
    out_color = inst.color;
    out_world = world.xyz;
}
