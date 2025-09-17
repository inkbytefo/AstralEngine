#version 450

// Bloom Vertex Shader
// Author: inkbytefo
// Project: AstralEngine



// Vertex shader inputs for full-screen quad
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;

// Outputs to fragment shader
layout(location = 0) out vec2 outTexCoord;
layout(location = 1) out vec2 outTexCoords[5]; // For sampling in bloom shader

// Push constants
layout(push_constant) uniform PushConstants {
    vec2 texelSize;
    int bloomPass; // 0: bright pass, 1: horizontal blur, 2: vertical blur, 3: composite
} pushConstants;

void main() {
    // Pass through texture coordinates
    outTexCoord = inTexCoord;
    
    // Calculate sampling coordinates for bloom
    // This creates a cross pattern for efficient sampling
    vec2 texelSize = pushConstants.texelSize;
    
    // Center sample
    outTexCoords[0] = inTexCoord;
    
    // Cross samples for blur
    outTexCoords[1] = inTexCoord + vec2(texelSize.x * 1.0, 0.0);
    outTexCoords[2] = inTexCoord + vec2(texelSize.x * -1.0, 0.0);
    outTexCoords[3] = inTexCoord + vec2(0.0, texelSize.y * 1.0);
    outTexCoords[4] = inTexCoord + vec2(0.0, texelSize.y * -1.0);
    
    // Output position (full-screen quad)
    gl_Position = vec4(inPosition.xy, 0.0, 1.0);
}
