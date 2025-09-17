// Vertex Attributes Definitions
// Author: inkbytefo
// Project: AstralEngine

#ifndef VERTEX_ATTRIBUTES_GLSL
#define VERTEX_ATTRIBUTES_GLSL

// Vertex attribute locations
#define POSITION_LOCATION    0
#define NORMAL_LOCATION      1
#define TEXCOORD_LOCATION    2
#define TANGENT_LOCATION     3
#define COLOR_LOCATION       4
#define JOINTS_LOCATION      5
#define WEIGHTS_LOCATION     6
#define INSTANCE_MATRIX0_LOCATION 7
#define INSTANCE_MATRIX1_LOCATION 8
#define INSTANCE_MATRIX2_LOCATION 9
#define INSTANCE_MATRIX3_LOCATION 10
#define INSTANCE_COLOR_LOCATION  11

// Vertex attribute input definitions
#define DEFINE_VERTEX_ATTRIBUTES() \
    layout(location = POSITION_LOCATION) in vec3 inPosition; \
    layout(location = NORMAL_LOCATION) in vec3 inNormal; \
    layout(location = TEXCOORD_LOCATION) in vec2 inTexCoord; \
    layout(location = TANGENT_LOCATION) in vec3 inTangent; \
    layout(location = COLOR_LOCATION) in vec4 inColor;

// Instanced rendering attributes
#define DEFINE_INSTANCE_ATTRIBUTES() \
    layout(location = INSTANCE_MATRIX0_LOCATION) in vec4 inInstanceMatrix0; \
    layout(location = INSTANCE_MATRIX1_LOCATION) in vec4 inInstanceMatrix1; \
    layout(location = INSTANCE_MATRIX2_LOCATION) in vec4 inInstanceMatrix2; \
    layout(location = INSTANCE_MATRIX3_LOCATION) in vec4 inInstanceMatrix3; \
    layout(location = INSTANCE_COLOR_LOCATION) in vec4 inInstanceColor;

// Skinning attributes
#define DEFINE_SKINNING_ATTRIBUTES() \
    layout(location = JOINTS_LOCATION) in ivec4 inJointIndices; \
    layout(location = WEIGHTS_LOCATION) in vec4 inJointWeights;

// Vertex attribute structure
struct VertexAttributes {
    vec3 position;
    vec3 normal;
    vec2 texCoord;
    vec3 tangent;
    vec4 color;
};

// Instance data structure
struct InstanceData {
    mat4 matrix;
    vec4 color;
};

// Skinning data structure
struct SkinningData {
    ivec4 jointIndices;
    vec4 jointWeights;
};

// Utility functions for vertex attributes
VertexAttributes GetVertexAttributes() {
    VertexAttributes attr;
    attr.position = inPosition;
    attr.normal = inNormal;
    attr.texCoord = inTexCoord;
    attr.tangent = inTangent;
    attr.color = inColor;
    return attr;
}

InstanceData GetInstanceData() {
    InstanceData data;
    data.matrix = mat4(
        inInstanceMatrix0,
        inInstanceMatrix1,
        inInstanceMatrix2,
        inInstanceMatrix3
    );
    data.color = inInstanceColor;
    return data;
}

SkinningData GetSkinningData() {
    SkinningData data;
    data.jointIndices = inJointIndices;
    data.jointWeights = inJointWeights;
    return data;
}

// Full-screen quad vertex generation
vec3 GetFullScreenQuadVertex(int vertexIndex) {
    switch (vertexIndex) {
        case 0: return vec3(-1.0, -1.0, 0.0); // Bottom left
        case 1: return vec3( 1.0, -1.0, 0.0); // Bottom right
        case 2: return vec3(-1.0,  1.0, 0.0); // Top left
        case 3: return vec3( 1.0,  1.0, 0.0); // Top right
        default: return vec3(0.0);
    }
}

vec2 GetFullScreenQuadTexCoord(int vertexIndex) {
    switch (vertexIndex) {
        case 0: return vec2(0.0, 0.0); // Bottom left
        case 1: return vec2(1.0, 0.0); // Bottom right
        case 2: return vec2(0.0, 1.0); // Top left
        case 3: return vec2(1.0, 1.0); // Top right
        default: return vec2(0.0);
    }
}

// Skybox cube vertices
vec3 GetSkyboxVertex(int vertexIndex) {
    const float scale = 1.0;
    switch (vertexIndex) {
        // Front face
        case 0: return vec3(-scale, -scale,  scale);
        case 1: return vec3( scale, -scale,  scale);
        case 2: return vec3(-scale,  scale,  scale);
        case 3: return vec3( scale,  scale,  scale);
        // Back face
        case 4: return vec3( scale, -scale, -scale);
        case 5: return vec3(-scale, -scale, -scale);
        case 6: return vec3( scale,  scale, -scale);
        case 7: return vec3(-scale,  scale, -scale);
        // Top face
        case 8: return vec3(-scale,  scale, -scale);
        case 9: return vec3( scale,  scale, -scale);
        case 10: return vec3(-scale,  scale,  scale);
        case 11: return vec3( scale,  scale,  scale);
        // Bottom face
        case 12: return vec3(-scale, -scale,  scale);
        case 13: return vec3( scale, -scale,  scale);
        case 14: return vec3(-scale, -scale, -scale);
        case 15: return vec3( scale, -scale, -scale);
        // Right face
        case 16: return vec3( scale, -scale,  scale);
        case 17: return vec3( scale, -scale, -scale);
        case 18: return vec3( scale,  scale,  scale);
        case 19: return vec3( scale,  scale, -scale);
        // Left face
        case 20: return vec3(-scale, -scale, -scale);
        case 21: return vec3(-scale, -scale,  scale);
        case 22: return vec3(-scale,  scale, -scale);
        case 23: return vec3(-scale,  scale,  scale);
        default: return vec3(0.0);
    }
}

// Vertex attribute validation
void ValidateVertexAttributes() {
    // Check if position is valid
    if (any(isinf(inPosition)) || any(isnan(inPosition))) {
        // Handle invalid position (could log error or set to default)
        // inPosition = vec3(0.0);
    }
    
    // Check if normal is valid
    if (any(isinf(inNormal)) || any(isnan(inNormal))) {
        // Handle invalid normal
        // inNormal = vec3(0.0, 1.0, 0.0);
    }
    
    // Check if texture coordinates are valid
    if (any(isinf(inTexCoord)) || any(isnan(inTexCoord))) {
        // Handle invalid texture coordinates
        // inTexCoord = vec2(0.0);
    }
}

// Vertex attribute compression utilities (for memory optimization)
vec2 PackTexCoord(vec2 texCoord) {
    // Pack texture coordinates into 16-bit precision
    return round(texCoord * 65535.0) / 65535.0;
}

vec2 UnpackTexCoord(vec2 packedTexCoord) {
    // Unpack texture coordinates from 16-bit precision
    return packedTexCoord;
}

vec3 PackNormal(vec3 normal) {
    // Pack normal into octahedral encoding
    vec2 octahedral = normal.xy / (abs(normal.x) + abs(normal.y) + abs(normal.z));
    if (normal.z < 0.0) {
        octahedral = (1.0 - abs(octahedral.yx)) * sign(octahedral.xy);
    }
    return vec3(octahedral, normal.z);
}

vec3 UnpackNormal(vec3 packedNormal) {
    // Unpack normal from octahedral encoding
    vec2 octahedral = packedNormal.xy;
    vec3 normal = vec3(octahedral, 1.0 - abs(octahedral.x) - abs(octahedral.y));
    float t = max(-normal.z, 0.0);
    normal.xy += t * sign(normal.xy) * (1.0 - abs(normal.xy));
    return normalize(normal);
}

#endif // VERTEX_ATTRIBUTES_GLSL
