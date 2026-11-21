#version 460
// Vertex pulling without buffers: positions and colors are constants indexed by gl_VertexIndex.
layout(push_constant) uniform Push { float time; } pc;
layout(location = 0) out vec3 out_color;

const vec2 positions[3] = vec2[](vec2(0.0, -0.6), vec2(0.6, 0.5), vec2(-0.6, 0.5));
const vec3 colors[3] = vec3[](vec3(1, 0.2, 0.2), vec3(0.2, 1, 0.2), vec3(0.2, 0.4, 1));

void main() {
    float c = cos(pc.time), s = sin(pc.time);
    vec2 p = mat2(c, s, -s, c) * positions[gl_VertexIndex];
    gl_Position = vec4(p, 0.0, 1.0);
    out_color = colors[gl_VertexIndex];
}
