#version 450

layout(set = 0, binding = 0) uniform GlobalUBO {
    mat4 view;
    mat4 proj;
    mat4 lightSpaceMatrix;
    vec4 viewPos;
    int lightCount;
    int hasShadows;
    int hasIBL;
} ubo;

layout(push_constant) uniform PushConstants {
    mat4 model;
} pc;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inTangent;
layout(location = 4) in vec3 inBitangent;

layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec3 fragViewPos;
layout(location = 3) out vec3 fragFragPos;
layout(location = 4) out mat3 TBN;

void main() {
    vec4 worldPos = pc.model * vec4(inPosition, 1.0);
    gl_Position = ubo.proj * ubo.view * worldPos;
    
    fragFragPos = worldPos.xyz;
    fragTexCoord = inTexCoord;
    fragViewPos = ubo.viewPos.xyz;
    
    // Normal Matrix
    mat3 normalMatrix = mat3(transpose(inverse(pc.model)));
    fragNormal = normalize(normalMatrix * inNormal);
    
    // TBN Matrix for Normal Mapping
    vec3 T = normalize(normalMatrix * inTangent);
    vec3 B = normalize(normalMatrix * inBitangent);
    vec3 N = normalize(normalMatrix * inNormal);
    TBN = mat3(T, B, N);
}
