#version 460
#include <astral/bindless.glsl>

struct Frame {
    mat4 view_projection;
    vec4 camera_position;
    vec4 light_direction;
    vec4 light_color;
};
layout(buffer_reference, scalar, buffer_reference_align = 4) buffer FrameRef { Frame frame; };
layout(buffer_reference, scalar, buffer_reference_align = 4) buffer Unused { uint data[]; };

layout(push_constant, scalar) uniform Push {
    Unused vertices;
    Unused instances;
    FrameRef frame;
    uint first_instance;
} pc;

layout(location = 0) in vec3 in_normal;
layout(location = 1) in vec4 in_color;
layout(location = 2) in vec3 in_world;
layout(location = 0) out vec4 out_color;

void main() {
    Frame f = pc.frame.frame;
    vec3 n = normalize(in_normal);
    vec3 to_light = -normalize(f.light_direction.xyz);
    float diffuse = max(dot(n, to_light), 0.0);
    vec3 lit = in_color.rgb * (f.light_color.rgb * diffuse + vec3(f.light_color.a));
    out_color = vec4(lit, in_color.a);
}
