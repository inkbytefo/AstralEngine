#version 450

// Tonemapping Vertex Shader
// Author: inkbytefo
// Project: AstralEngine



// Vertex shader inputs for full-screen quad
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;

// Outputs to fragment shader
layout(location = 0) out vec2 outTexCoord;
layout(location = 1) out vec3 outViewRay;

// Uniform buffer objects
layout(binding = 0) uniform UniformBufferObject {
    mat4 projection;
    mat4 view;
    vec3 cameraPosition;
    float time;
} ubo;

// Push constants
layout(push_constant) uniform PushConstants {
    vec2 texelSize;
    int useBloom;
    int useVignette;
    int useChromaticAberration;
} pushConstants;

void main() {
    // Pass through texture coordinates
    outTexCoord = inTexCoord;
    
    // Calculate view ray for effects like depth of field
    // This creates rays from camera through each pixel
    vec4 clipPos = vec4(inPosition.xy, 1.0, 1.0);
    vec4 viewPos = inverse(ubo.projection) * clipPos;
    viewPos = vec4(viewPos.xy, -1.0, 0.0); // Point towards negative Z
    
    // Transform to world space
    mat4 invView = inverse(ubo.view);
    vec4 worldPos = invView * viewPos;
    outViewRay = normalize(worldPos.xyz);
    
    // Output position (full-screen quad)
    gl_Position = vec4(inPosition.xy, 0.0, 1.0);
}
