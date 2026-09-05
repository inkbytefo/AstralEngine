#version 460
//
// TAAResolve.glsl — Temporal Anti-Aliasing Resolve & Tonemapping Pass (Faz 3)
//
// Yenilikler (Faz 3 - Secenek A):
// 1. g_MotionVectors ile alt-piksel geriye yansitma (Subpixel Reprojection).
// 2. YCoCg renk uzayinda 3x3 Varyans Kirpmasi (Variance / AABB Clipping) — sifir ghosting.
// 3. Catmull-Rom 5-tap bicubic filtreleme ile donanimsal linear ornekleme — sifir temporal bulaniklik.
// 4. Luminance weighting ile anti-flicker ve parlak nokta (specular highlight) kararliligi.
// 5. Dogrusal HDR ciktisi (outHistory) ve ACES Filmik Tonemapping + sRGB gamma (outImage).
//

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(binding = 0, rgba16f) uniform readonly image2D currImage;
layout(binding = 1) uniform sampler2D historyTexture;
layout(binding = 2, rgba8)   uniform writeonly image2D outImage;
layout(binding = 3, rgba16f) uniform writeonly image2D outHistory;
layout(binding = 4, rg16f)   uniform readonly image2D motionVectors;

layout(push_constant) uniform TAAPushConstants {
    vec4 screenRes; // x: width, y: height, z: frameIndex (0 = reset), w: blendAlpha (negatif ise debug bypass)
};

// ACES Filmik Tonemapping Operatoru
vec3 acesTonemap(vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

// RGB <-> YCoCg Donusumleri (Unreal Engine / Call of Duty TAA standardi)
vec3 RGB_to_YCoCg(vec3 c) {
    return vec3(
         0.25 * c.r + 0.5 * c.g + 0.25 * c.b,
         0.5  * c.r             - 0.5  * c.b,
        -0.25 * c.r + 0.5 * c.g - 0.25 * c.b
    );
}

vec3 YCoCg_to_RGB(vec3 c) {
    return vec3(
        c.x + c.y - c.z,
        c.x + c.z,
        c.x - c.y - c.z
    );
}

// Ray-AABB intersection for Variance Clipping in YCoCg space
vec3 clipAABB(vec3 aabbMin, vec3 aabbMax, vec3 p, vec3 q) {
    vec3 r = q - p;
    vec3 t0 = (aabbMin - p) / (r + vec3(1e-7));
    vec3 t1 = (aabbMax - p) / (r + vec3(1e-7));
    vec3 tmax = max(t0, t1);
    float t = clamp(min(min(tmax.x, tmax.y), tmax.z), 0.0, 1.0);
    return mix(p, q, t);
}

// Catmull-Rom 5-Tap Bicubic Filter using Hardware Bilinear Samples (Filament / Jimenez)
vec4 sampleCatmullRom5Tap(sampler2D tex, vec2 uv, vec2 texSize) {
    vec2 samplePos = uv * texSize;
    vec2 tc = floor(samplePos - 0.5) + 0.5;
    vec2 f = samplePos - tc;
    vec2 f2 = f * f;
    vec2 f3 = f2 * f;

    vec2 w0 = f2 - 0.5 * (f3 + f);
    vec2 w1 = 1.5 * f3 - 2.5 * f2 + 1.0;
    vec2 w3 = 0.5 * (f3 - f2);
    vec2 w2 = 1.0 - w0 - w1 - w3;

    vec2 w12 = w1 + w2;
    vec2 tc12 = tc + w2 / (w12 + vec2(1e-7));

    vec2 tc0 = tc - 1.0;
    vec2 tc3 = tc + 2.0;

    vec4 s = vec4(0.0);
    s += textureLod(tex, vec2(tc12.x, tc0.y) / texSize, 0.0) * (w12.x * w0.y);
    s += textureLod(tex, vec2(tc0.x, tc12.y) / texSize, 0.0) * (w0.x * w12.y);
    s += textureLod(tex, vec2(tc12.x, tc12.y) / texSize, 0.0) * (w12.x * w12.y);
    s += textureLod(tex, vec2(tc3.x, tc12.y) / texSize, 0.0) * (w3.x * w12.y);
    s += textureLod(tex, vec2(tc12.x, tc3.y) / texSize, 0.0) * (w12.x * w3.y);

    float totalWeight = (w12.x * w0.y) + (w0.x * w12.y) + (w12.x * w12.y) + (w3.x * w12.y) + (w12.x * w3.y);
    return s / max(totalWeight, 1e-7);
}

void main() {
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
    ivec2 res = ivec2(screenRes.xy);

    if (pixel.x >= res.x || pixel.y >= res.y) return;

    vec3 curr = imageLoad(currImage, pixel).rgb;

    // Debug Pass-through modu: Eger w < 0 ise dogrudan tonemap'siz aktar
    if (screenRes.w < 0.0) {
        imageStore(outImage, pixel, vec4(curr, 1.0));
        imageStore(outHistory, pixel, vec4(curr, 1.0));
        return;
    }

    float alpha = screenRes.w;
    if (screenRes.z < 0.5) {
        alpha = 1.0; // Ilk karede tarihceyi yok say
    }

    // Ekran uzayi UV ve Hiz Vektoru
    vec2 currUV = (vec2(pixel) + 0.5) / vec2(res);
    vec2 motion = imageLoad(motionVectors, pixel).xy;
    if (isnan(motion.x) || isnan(motion.y) || isinf(motion.x) || isinf(motion.y)) {
        motion = vec2(0.0);
    }
    vec2 prevUV = currUV - motion;

    // Eger geriye yansitilan UV ekran disindaysa veya gecersizse tarihceyi reddet
    if (isnan(prevUV.x) || isnan(prevUV.y) || isinf(prevUV.x) || isinf(prevUV.y) ||
        prevUV.x < 0.0 || prevUV.x > 1.0 || prevUV.y < 0.0 || prevUV.y > 1.0) {
        alpha = 1.0;
    }

    vec3 blendedHDR = curr;

    if (alpha < 0.999) {
        // 1. 3x3 Komsuluk Renk Momentleri (YCoCg Uzayinda)
        vec3 m1 = vec3(0.0);
        vec3 m2 = vec3(0.0);
        vec3 minNeighbor = vec3(1e9);
        vec3 maxNeighbor = vec3(-1e9);

        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                ivec2 np = clamp(pixel + ivec2(dx, dy), ivec2(0), res - 1);
                vec3 cRGB = imageLoad(currImage, np).rgb;
                vec3 cYCoCg = RGB_to_YCoCg(cRGB);
                m1 += cYCoCg;
                m2 += cYCoCg * cYCoCg;
                minNeighbor = min(minNeighbor, cYCoCg);
                maxNeighbor = max(maxNeighbor, cYCoCg);
            }
        }

        vec3 mu = m1 / 9.0;
        vec3 sigma = sqrt(max(m2 / 9.0 - mu * mu, vec3(0.0)));

        // Variance Clipping AABB (Karis / Jimenez varyans kirpma sinirlari)
        const float gamma = 1.25;
        vec3 aabbMin = max(mu - gamma * sigma, minNeighbor);
        vec3 aabbMax = min(mu + gamma * sigma, maxNeighbor);

        // 2. Onceki kareden Catmull-Rom 5-Tap Bicubic Ornekleme
        vec4 histSample = sampleCatmullRom5Tap(historyTexture, prevUV, vec2(res));
        if (isnan(histSample.r) || isnan(histSample.g) || isnan(histSample.b) ||
            isinf(histSample.r) || isinf(histSample.g) || isinf(histSample.b)) {
            histSample = vec4(curr, 1.0);
        }
        vec3 histRGB = max(histSample.rgb, vec3(0.0));
        vec3 histYCoCg = RGB_to_YCoCg(histRGB);

        // 3. Tarihceyi AABB'ye Kirp (Variance Clipping)
        vec3 currYCoCg = RGB_to_YCoCg(curr);
        vec3 clippedYCoCg = clipAABB(aabbMin, aabbMax, currYCoCg, histYCoCg);
        vec3 clampedHistRGB = max(YCoCg_to_RGB(clippedYCoCg), vec3(0.0));

        // 4. Luminance Weighting (Anti-Flicker)
        float wCurr = 1.0 / (1.0 + currYCoCg.x);
        float wHist = 1.0 / (1.0 + clippedYCoCg.x);

        blendedHDR = (clampedHistRGB * wHist * (1.0 - alpha) + curr * wCurr * alpha) /
                     max(wHist * (1.0 - alpha) + wCurr * alpha, 1e-7);

        if (isnan(blendedHDR.r) || isnan(blendedHDR.g) || isnan(blendedHDR.b) ||
            isinf(blendedHDR.r) || isinf(blendedHDR.g) || isinf(blendedHDR.b)) {
            blendedHDR = curr;
        }
    }

    // Bir sonraki kare icin saf Dogrusal HDR tarihceye yazilir
    imageStore(outHistory, pixel, vec4(blendedHDR, 1.0));

    // Nihai goruntu icin ACES Tonemapping ve Gamma (sRGB) uygulanir
    vec3 tonemapped = acesTonemap(blendedHDR);
    tonemapped = pow(tonemapped, vec3(1.0 / 2.2));

    imageStore(outImage, pixel, vec4(tonemapped, 1.0));
}
