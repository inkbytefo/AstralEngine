// Camera and Matrix Utilities
// Author: inkbytefo
// Project: AstralEngine

#ifndef CAMERA_GLSL
#define CAMERA_GLSL

#include "common.glsl"

// Camera structure
struct Camera {
    mat4 view;
    mat4 projection;
    mat4 viewProjection;
    vec3 position;
    vec3 direction;
    vec3 up;
    vec3 right;
    float near;
    float far;
    float fov;
    float aspectRatio;
};

// Camera types
const int CAMERA_TYPE_PERSPECTIVE = 0;
const int CAMERA_TYPE_ORTHOGRAPHIC = 1;
const int CAMERA_TYPE_CUSTOM = 2;

// Matrix creation functions
mat4 CreateIdentityMatrix() {
    return mat4(
        1.0, 0.0, 0.0, 0.0,
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        0.0, 0.0, 0.0, 1.0
    );
}

mat4 CreateTranslationMatrix(vec3 translation) {
    return mat4(
        1.0, 0.0, 0.0, 0.0,
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        translation.x, translation.y, translation.z, 1.0
    );
}

mat4 CreateScaleMatrix(vec3 scale) {
    return mat4(
        scale.x, 0.0, 0.0, 0.0,
        0.0, scale.y, 0.0, 0.0,
        0.0, 0.0, scale.z, 0.0,
        0.0, 0.0, 0.0, 1.0
    );
}

mat4 CreateRotationMatrixX(float angle) {
    float c = cos(angle);
    float s = sin(angle);
    return mat4(
        1.0, 0.0, 0.0, 0.0,
        0.0, c, s, 0.0,
        0.0, -s, c, 0.0,
        0.0, 0.0, 0.0, 1.0
    );
}

mat4 CreateRotationMatrixY(float angle) {
    float c = cos(angle);
    float s = sin(angle);
    return mat4(
        c, 0.0, -s, 0.0,
        0.0, 1.0, 0.0, 0.0,
        s, 0.0, c, 0.0,
        0.0, 0.0, 0.0, 1.0
    );
}

mat4 CreateRotationMatrixZ(float angle) {
    float c = cos(angle);
    float s = sin(angle);
    return mat4(
        c, s, 0.0, 0.0,
        -s, c, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        0.0, 0.0, 0.0, 1.0
    );
}

mat4 CreateRotationMatrix(vec3 rotation) {
    mat4 rotX = CreateRotationMatrixX(rotation.x);
    mat4 rotY = CreateRotationMatrixY(rotation.y);
    mat4 rotZ = CreateRotationMatrixZ(rotation.z);
    return rotZ * rotY * rotX;
}

mat4 CreateLookAtMatrix(vec3 eye, vec3 center, vec3 up) {
    vec3 f = normalize(center - eye);
    vec3 s = normalize(cross(f, up));
    vec3 u = cross(s, f);
    
    return mat4(
        s.x, u.x, -f.x, 0.0,
        s.y, u.y, -f.y, 0.0,
        s.z, u.z, -f.z, 0.0,
        -dot(s, eye), -dot(u, eye), dot(f, eye), 1.0
    );
}

mat4 CreatePerspectiveMatrix(float fov, float aspect, float near, float far) {
    float f = 1.0 / tan(fov * 0.5);
    float rangeInv = 1.0 / (near - far);
    
    return mat4(
        f / aspect, 0.0, 0.0, 0.0,
        0.0, f, 0.0, 0.0,
        0.0, 0.0, (near + far) * rangeInv, -1.0,
        0.0, 0.0, near * far * rangeInv * 2.0, 0.0
    );
}

mat4 CreateOrthographicMatrix(float left, float right, float bottom, float top, float near, float far) {
    return mat4(
        2.0 / (right - left), 0.0, 0.0, 0.0,
        0.0, 2.0 / (top - bottom), 0.0, 0.0,
        0.0, 0.0, -2.0 / (far - near), 0.0,
        -(right + left) / (right - left), -(top + bottom) / (top - bottom), -(far + near) / (far - near), 1.0
    );
}

// Camera utility functions
Camera CreateCamera(vec3 position, vec3 target, vec3 up, float fov, float aspect, float near, float far) {
    Camera camera;
    camera.position = position;
    camera.direction = normalize(target - position);
    camera.up = normalize(up);
    camera.right = normalize(cross(camera.direction, camera.up));
    camera.up = normalize(cross(camera.right, camera.direction)); // Recalculate up
    camera.near = near;
    camera.far = far;
    camera.fov = fov;
    camera.aspectRatio = aspect;
    
    camera.view = CreateLookAtMatrix(position, target, up);
    camera.projection = CreatePerspectiveMatrix(fov, aspect, near, far);
    camera.viewProjection = camera.projection * camera.view;
    
    return camera;
}

Camera CreateOrthographicCamera(vec3 position, vec3 target, vec3 up, float size, float aspect, float near, float far) {
    Camera camera;
    camera.position = position;
    camera.direction = normalize(target - position);
    camera.up = normalize(up);
    camera.right = normalize(cross(camera.direction, camera.up));
    camera.up = normalize(cross(camera.right, camera.direction));
    camera.near = near;
    camera.far = far;
    camera.fov = 0.0; // Not used for orthographic
    camera.aspectRatio = aspect;
    
    float halfSize = size * 0.5;
    camera.view = CreateLookAtMatrix(position, target, up);
    camera.projection = CreateOrthographicMatrix(-halfSize * aspect, halfSize * aspect, -halfSize, halfSize, near, far);
    camera.viewProjection = camera.projection * camera.view;
    
    return camera;
}

// Matrix operations
mat4 InverseMatrix(mat4 m) {
    // This is a simplified inverse - for production use, you'd want a more robust implementation
    float det = determinant(m);
    if (abs(det) < EPSILON) {
        return mat4(1.0); // Return identity if singular
    }
    return inverse(m);
}

mat4 TransposeMatrix(mat4 m) {
    return transpose(m);
}

mat3 CreateNormalMatrix(mat4 modelMatrix) {
    return transpose(inverse(mat3(modelMatrix)));
}

// Screen space transformations
vec3 WorldToScreen(vec3 worldPos, mat4 viewProjection) {
    vec4 clipPos = viewProjection * vec4(worldPos, 1.0);
    vec3 ndcPos = clipPos.xyz / clipPos.w;
    return ndcPos * 0.5 + 0.5; // Convert to [0, 1] range
}

vec3 ScreenToWorld(vec3 screenPos, mat4 inverseViewProjection) {
    vec4 ndcPos = vec4(screenPos * 2.0 - 1.0, screenPos.z * 2.0 - 1.0, 1.0);
    vec4 worldPos = inverseViewProjection * ndcPos;
    return worldPos.xyz / worldPos.w;
}

vec2 WorldToScreenUV(vec3 worldPos, mat4 viewProjection) {
    vec4 clipPos = viewProjection * vec4(worldPos, 1.0);
    vec2 ndcPos = clipPos.xy / clipPos.w;
    return ndcPos * 0.5 + 0.5;
}

// Ray casting
vec3 GetCameraRay(vec2 screenUV, Camera camera) {
    // Calculate ray in clip space
    vec4 rayClip = vec4(screenUV * 2.0 - 1.0, -1.0, 1.0);
    
    // Transform to camera space
    vec4 rayCamera = inverse(camera.projection) * rayClip;
    rayCamera = vec4(rayCamera.xy, -1.0, 0.0);
    
    // Transform to world space
    vec4 rayWorld = inverse(camera.view) * rayCamera;
    
    return normalize(rayWorld.xyz);
}

// Camera frustum
struct Frustum {
    vec4 planes[6]; // left, right, bottom, top, near, far
};

Frustum CreateFrustum(Camera camera) {
    Frustum frustum;
    
    mat4 vp = camera.viewProjection;
    
    // Extract frustum planes from view-projection matrix
    // Left plane
    frustum.planes[0] = vec4(vp[0][3] + vp[0][0], vp[1][3] + vp[1][0], vp[2][3] + vp[2][0], vp[3][3] + vp[3][0]);
    // Right plane
    frustum.planes[1] = vec4(vp[0][3] - vp[0][0], vp[1][3] - vp[1][0], vp[2][3] - vp[2][0], vp[3][3] - vp[3][0]);
    // Bottom plane
    frustum.planes[2] = vec4(vp[0][3] + vp[0][1], vp[1][3] + vp[1][1], vp[2][3] + vp[2][1], vp[3][3] + vp[3][1]);
    // Top plane
    frustum.planes[3] = vec4(vp[0][3] - vp[0][1], vp[1][3] - vp[1][1], vp[2][3] - vp[2][1], vp[3][3] - vp[3][1]);
    // Near plane
    frustum.planes[4] = vec4(vp[0][3] + vp[0][2], vp[1][3] + vp[1][2], vp[2][3] + vp[2][2], vp[3][3] + vp[3][2]);
    // Far plane
    frustum.planes[5] = vec4(vp[0][3] - vp[0][2], vp[1][3] - vp[1][2], vp[2][3] - vp[2][2], vp[3][3] - vp[3][2]);
    
    // Normalize planes
    for (int i = 0; i < 6; i++) {
        frustum.planes[i] /= length(frustum.planes[i].xyz);
    }
    
    return frustum;
}

bool IsPointInFrustum(vec3 point, Frustum frustum) {
    for (int i = 0; i < 6; i++) {
        if (dot(frustum.planes[i].xyz, point) + frustum.planes[i].w < 0.0) {
            return false;
        }
    }
    return true;
}

bool IsSphereInFrustum(vec3 center, float radius, Frustum frustum) {
    for (int i = 0; i < 6; i++) {
        if (dot(frustum.planes[i].xyz, center) + frustum.planes[i].w < -radius) {
            return false;
        }
    }
    return true;
}

// Camera animation utilities
vec3 AnimateCameraPosition(vec3 basePosition, float time, float radius, float height) {
    float angle = time * 0.5; // Slow rotation
    vec3 offset = vec3(cos(angle) * radius, height, sin(angle) * radius);
    return basePosition + offset;
}

Camera CreateOrbitCamera(vec3 target, float radius, float height, float angle, float fov, float aspect, float near, float far) {
    vec3 position = target + vec3(cos(angle) * radius, height, sin(angle) * radius);
    return CreateCamera(position, target, vec3(0.0, 1.0, 0.0), fov, aspect, near, far);
}

// Depth utilities
float LinearizeDepth(float depth, Camera camera) {
    float z = depth * 2.0 - 1.0;
    return (2.0 * camera.near * camera.far) / (camera.far + camera.near - z * (camera.far - camera.near));
}

float GetViewSpaceDepth(vec3 worldPos, Camera camera) {
    vec4 viewPos = camera.view * vec4(worldPos, 1.0);
    return -viewPos.z; // Negative because camera looks down -Z
}

#endif // CAMERA_GLSL
