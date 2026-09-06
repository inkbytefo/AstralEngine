#version 460

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(binding = 0, r8) uniform writeonly image2D outInvalidationMask;

layout(push_constant) uniform InvalidationPushConstants {
    vec4 screenRes;     // xy: extent, z: numRects, w: isGlobalChange
    vec4 changedRect0; // xy: minUV, zw: maxUV
    vec4 changedRect1; // xy: minUV, zw: maxUV
    vec4 changedRect2; // xy: minUV, zw: maxUV
    vec4 changedRect3; // xy: minUV, zw: maxUV
};

bool isInside(vec2 uv, vec4 r) {
    return uv.x >= r.x && uv.x <= r.z && uv.y >= r.y && uv.y <= r.w;
}

void main() {
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
    ivec2 res = ivec2(screenRes.xy);

    if (pixel.x >= res.x || pixel.y >= res.y) return;

    if (screenRes.w > 0.5) {
        // Global invalidation: entire screen invalidated
        imageStore(outInvalidationMask, pixel, vec4(1.0));
        return;
    }

    vec2 uv = (vec2(pixel) + 0.5) / vec2(res);
    int numRects = int(screenRes.z);
    bool invalidated = false;

    if (numRects > 0 && isInside(uv, changedRect0)) invalidated = true;
    if (numRects > 1 && isInside(uv, changedRect1)) invalidated = true;
    if (numRects > 2 && isInside(uv, changedRect2)) invalidated = true;
    if (numRects > 3 && isInside(uv, changedRect3)) invalidated = true;

    imageStore(outInvalidationMask, pixel, vec4(invalidated ? 1.0 : 0.0));
}
