#version 460
#include <astral/bindless.glsl>
// Debug lines: two vertices per line, read from a buffer (position.xyz, color).
struct LineVertex { vec4 position; vec4 color; };
ASTRAL_BUFFER(LineVertices, LineVertex);
layout(push_constant, scalar) uniform Push { mat4 view_projection; LineVertices vertices; } pc;
layout(location = 0) out vec4 out_color;
void main() {
    LineVertex v = pc.vertices.data[gl_VertexIndex];
    gl_Position = pc.view_projection * vec4(v.position.xyz, 1.0);
    out_color = v.color;
}
