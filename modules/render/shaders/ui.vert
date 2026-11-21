#version 460
#include <astral/bindless.glsl>
// 2D overlay: indices and vertices pulled from buffers; positions in pixels -> clip space.
struct UiVertex { vec2 position; vec2 uv; uint color; };
ASTRAL_BUFFER(UiVertices, UiVertex);
ASTRAL_BUFFER(UiIndices, uint);
layout(push_constant, scalar) uniform Push {
    vec2 scale; vec2 translate; UiVertices vertices; UiIndices indices;
    int vertex_offset; uint texture; uint flags; float sdf_softness;
} pc;
layout(location = 0) out vec2 out_uv;
layout(location = 1) out vec4 out_color;
void main() {
    uint index = pc.indices.data[gl_VertexIndex];
    UiVertex v = pc.vertices.data[int(index) + pc.vertex_offset];
    gl_Position = vec4(v.position * pc.scale + pc.translate, 0.0, 1.0);
    out_uv = v.uv;
    out_color = unpackUnorm4x8(v.color);
}
