#version 460
// Hard-coded triangle around the center of the screen; no vertex buffers.
const vec2 positions[3] = vec2[](vec2(0.0, -0.5), vec2(0.5, 0.5), vec2(-0.5, 0.5));
void main() { gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0); }
