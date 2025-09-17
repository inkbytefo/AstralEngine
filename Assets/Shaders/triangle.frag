#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 2) in vec2 inTexCoord;

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) out vec4 outColor;

void main() {
    // Texture sampling - daha güvenli yaklaşım
    vec4 texColor = texture(texSampler, inTexCoord);
    
    // Eğer texture geçersizse (alpha çok düşükse) sabit renk kullan
    if (texColor.a < 0.1) {
        // Fallback olarak vertex shader'den gelen beyaz rengi kullan
        outColor = vec4(fragColor, 1.0);
    } else {
        // Texture'i kullan
        outColor = vec4(texColor.rgb * fragColor, texColor.a);
    }
}
