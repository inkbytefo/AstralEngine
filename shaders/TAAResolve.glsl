#version 460

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(binding = 0, rgba8) uniform readonly image2D currImage;
layout(binding = 1, rgba8) uniform readonly image2D historyImage;
layout(binding = 2, rgba8) uniform writeonly image2D outImage;

layout(push_constant) uniform TAAPushConstants {
    vec4 screenRes; // x: width, y: height, z: frameIndex (0 = reset history), w: blendAlpha (0.12)
};

void main() {
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
    ivec2 res = ivec2(screenRes.xy);

    if (pixel.x >= res.x || pixel.y >= res.y) return;

    vec3 curr = imageLoad(currImage, pixel).rgb;

    // 3x3 Renk Komşuluğu Sınır Kutusu (Neighborhood Color Bounding Box)
    // Hareketli nesnelerdeki hayalet/gölgeleme (ghosting) etkisini tamamen yok eder
    vec3 cmin = curr;
    vec3 cmax = curr;

    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            ivec2 np = clamp(pixel + ivec2(dx, dy), ivec2(0), res - 1);
            vec3 c = imageLoad(currImage, np).rgb;
            cmin = min(cmin, c);
            cmax = max(cmax, c);
        }
    }

    vec3 hist = imageLoad(historyImage, pixel).rgb;

    // Tarihçeyi mevcut pikselin komşuluk sınırlarına kırp
    vec3 clampedHist = clamp(hist, cmin, cmax);

    // Exponential Moving Average (EMA) Harmanlama
    float alpha = screenRes.w;
    if (screenRes.z < 0.5) {
        alpha = 1.0; // Ilk karede tarihceyi yok say
    }

    vec3 finalColor = mix(clampedHist, curr, alpha);

    imageStore(outImage, pixel, vec4(finalColor, 1.0));
}
