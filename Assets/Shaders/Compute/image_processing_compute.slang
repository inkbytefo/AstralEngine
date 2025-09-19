// Image Processing Compute Shader
// Author: inkbytefo
// Project: AstralEngine
// Description: Image processing operations using SLANG compute shader

// Constants
static const float PI = 3.14159265359;

// Image processing operations
static const int OP_NONE = 0;
static const int OP_BLUR = 1;
static const int OP_SHARPEN = 2;
static const int OP_EDGE_DETECT = 3;
static const int OP_EMBOSS = 4;
static const int OP_DILATE = 5;
static const int OP_ERODE = 6;
static const int OP_BILATERAL_FILTER = 7;
static const int OP_MORPHOLOGICAL_GRADIENT = 8;

struct ImageProcessingUBO {
    int operation;
    float kernel[25]; // 5x5 kernel
    float kernelWeight;
    float sigma;       // For Gaussian blur
    float threshold;   // For edge detection
    float2 texelSize;
    int padding[2];
};

// Textures
Texture2D inputTexture : register(t0);
Texture2D secondaryTexture : register(t1); // For some operations
RWTexture2D<float4> outputImage : register(u0);

SamplerState linearSampler : register(s0);

// Gaussian kernel generation
float Gaussian(float x, float sigma)
{
    return exp(-(x * x) / (2.0 * sigma * sigma)) / (sqrt(2.0 * PI) * sigma);
}

// Sobel edge detection kernels
static const float sobelX[9] = {
    -1.0, 0.0, 1.0,
    -2.0, 0.0, 2.0,
    -1.0, 0.0, 1.0
};

static const float sobelY[9] = {
    -1.0, -2.0, -1.0,
     0.0,  0.0,  0.0,
     1.0,  2.0,  1.0
};

// Convolution operation
float4 Convolution(Texture2D tex, float2 uv, float2 texelSize, float kernel[9])
{
    float4 result = float4(0.0, 0.0, 0.0, 0.0);
    
    for (int i = -1; i <= 1; i++) {
        for (int j = -1; j <= 1; j++) {
            float2 offset = float2(float(j), float(i)) * texelSize;
            float weight = kernel[(i + 1) * 3 + (j + 1)];
            result += tex.SampleLevel(linearSampler, uv + offset, 0) * weight;
        }
    }
    
    return result;
}

// 5x5 convolution for more advanced operations
float4 Convolution5x5(Texture2D tex, float2 uv, float2 texelSize, float kernel[25])
{
    float4 result = float4(0.0, 0.0, 0.0, 0.0);
    
    for (int i = -2; i <= 2; i++) {
        for (int j = -2; j <= 2; j++) {
            float2 offset = float2(float(j), float(i)) * texelSize;
            float weight = kernel[(i + 2) * 5 + (j + 2)];
            result += tex.SampleLevel(linearSampler, uv + offset, 0) * weight;
        }
    }
    
    return result;
}

// Bilateral filter
float4 BilateralFilter(Texture2D tex, float2 uv, float2 texelSize, float sigmaSpatial, float sigmaRange)
{
    float4 centerColor = tex.SampleLevel(linearSampler, uv, 0);
    float4 result = float4(0.0, 0.0, 0.0, 0.0);
    float totalWeight = 0.0;
    
    for (int i = -2; i <= 2; i++) {
        for (int j = -2; j <= 2; j++) {
            float2 offset = float2(float(j), float(i)) * texelSize;
            float4 sampleColor = tex.SampleLevel(linearSampler, uv + offset, 0);
            
            // Spatial weight
            float spatialDist = length(float2(float(i), float(j)));
            float spatialWeight = Gaussian(spatialDist, sigmaSpatial);
            
            // Range weight
            float rangeDist = length(centerColor.rgb - sampleColor.rgb);
            float rangeWeight = Gaussian(rangeDist, sigmaRange);
            
            float weight = spatialWeight * rangeWeight;
            result += sampleColor * weight;
            totalWeight += weight;
        }
    }
    
    return result / totalWeight;
}

// Morphological operations
float4 MorphologicalOperation(Texture2D tex, float2 uv, float2 texelSize, int operation)
{
    float4 result = float4(0.0, 0.0, 0.0, 0.0);
    
    // 3x3 neighborhood
    float4 samples[9];
    int index = 0;
    for (int i = -1; i <= 1; i++) {
        for (int j = -1; j <= 1; j++) {
            float2 offset = float2(float(j), float(i)) * texelSize;
            samples[index++] = tex.SampleLevel(linearSampler, uv + offset, 0);
        }
    }
    
    if (operation == OP_DILATE) {
        // Dilation: take maximum
        result = samples[0];
        for (int i = 1; i < 9; i++) {
            result = max(result, samples[i]);
        }
    } else if (operation == OP_ERODE) {
        // Erosion: take minimum
        result = samples[0];
        for (int i = 1; i < 9; i++) {
            result = min(result, samples[i]);
        }
    } else if (operation == OP_MORPHOLOGICAL_GRADIENT) {
        // Morphological gradient: dilation - erosion
        float4 dilation = samples[0];
        float4 erosion = samples[0];
        for (int i = 1; i < 9; i++) {
            dilation = max(dilation, samples[i]);
            erosion = min(erosion, samples[i]);
        }
        result = dilation - erosion;
    }
    
    return result;
}

// Edge detection using Sobel operator
float4 EdgeDetection(Texture2D tex, float2 uv, float2 texelSize, float threshold)
{
    // Apply Sobel X and Y
    float4 edgeX = Convolution(tex, uv, texelSize, sobelX);
    float4 edgeY = Convolution(tex, uv, texelSize, sobelY);
    
    // Calculate magnitude
    float4 magnitude = sqrt(edgeX * edgeX + edgeY * edgeY);
    
    // Apply threshold
    if (threshold > 0.0) {
        magnitude = step(float4(threshold, threshold, threshold, threshold), magnitude);
    }
    
    return magnitude;
}

// Emboss effect
float4 Emboss(Texture2D tex, float2 uv, float2 texelSize)
{
    static const float embossKernel[9] = {
        -2.0, -1.0,  0.0,
        -1.0,  1.0,  1.0,
         0.0,  1.0,  2.0
    };
    
    float4 result = Convolution(tex, uv, texelSize, embossKernel);
    result += 0.5; // Add bias to make it visible
    
    return result;
}

// Sharpen filter
float4 Sharpen(Texture2D tex, float2 uv, float2 texelSize)
{
    static const float sharpenKernel[9] = {
         0.0, -1.0,  0.0,
        -1.0,  5.0, -1.0,
         0.0, -1.0,  0.0
    };
    
    return Convolution(tex, uv, texelSize, sharpenKernel);
}

// Custom blur using provided kernel
float4 CustomBlur(Texture2D tex, float2 uv, float2 texelSize, float kernel[25], float kernelWeight)
{
    return Convolution5x5(tex, uv, texelSize, kernel) / kernelWeight;
}

// Image blending operations
float4 BlendImages(float4 color1, float4 color2, int blendMode)
{
    switch (blendMode) {
        case 0: // Normal
            return color2;
        case 1: // Multiply
            return color1 * color2;
        case 2: // Screen
            return float4(1.0, 1.0, 1.0, 1.0) - (float4(1.0, 1.0, 1.0, 1.0) - color1) * (float4(1.0, 1.0, 1.0, 1.0) - color2);
        case 3: // Overlay
            return lerp(color1 * color2 * 2.0, 
                       float4(1.0, 1.0, 1.0, 1.0) - 2.0 * (float4(1.0, 1.0, 1.0, 1.0) - color1) * (float4(1.0, 1.0, 1.0, 1.0) - color2), 
                       step(0.5, color1));
        case 4: // Soft Light
            return lerp(2.0 * color1 * color2 + color1 * color1 * (float4(1.0, 1.0, 1.0, 1.0) - 2.0 * color2), 
                       sqrt(color1) * (2.0 * color2 - float4(1.0, 1.0, 1.0, 1.0)) + 2.0 * color1 * (float4(1.0, 1.0, 1.0, 1.0) - color2), 
                       step(0.5, color2));
        default:
            return color2;
    }
}

// Color adjustments
float4 AdjustColor(float4 color, float brightness, float contrast, float saturation)
{
    // Brightness
    color.rgb += brightness;
    
    // Contrast
    color.rgb = ((color.rgb - float3(0.5, 0.5, 0.5)) * max(contrast, 0.0)) + float3(0.5, 0.5, 0.5);
    
    // Saturation
    float luminance = dot(color.rgb, float3(0.2126, 0.7152, 0.0722));
    color.rgb = lerp(float3(luminance, luminance, luminance), color.rgb, max(saturation, 0.0));
    
    return color;
}

[shader("compute")]
[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID,
          ImageProcessingUBO ipSettings : register(b0))
{
    int2 texelCoord = int2(dispatchThreadID.xy);
    float2 uv = float2(texelCoord) * ipSettings.texelSize;
    
    // Sample input texture
    float4 color = inputTexture.SampleLevel(linearSampler, uv, 0);
    
    // Apply selected operation
    switch (ipSettings.operation) {
        case OP_BLUR:
            color = CustomBlur(inputTexture, uv, ipSettings.texelSize, ipSettings.kernel, ipSettings.kernelWeight);
            break;
            
        case OP_SHARPEN:
            color = Sharpen(inputTexture, uv, ipSettings.texelSize);
            break;
            
        case OP_EDGE_DETECT:
            color = EdgeDetection(inputTexture, uv, ipSettings.texelSize, ipSettings.threshold);
            break;
            
        case OP_EMBOSS:
            color = Emboss(inputTexture, uv, ipSettings.texelSize);
            break;
            
        case OP_DILATE:
            color = MorphologicalOperation(inputTexture, uv, ipSettings.texelSize, OP_DILATE);
            break;
            
        case OP_ERODE:
            color = MorphologicalOperation(inputTexture, uv, ipSettings.texelSize, OP_ERODE);
            break;
            
        case OP_BILATERAL_FILTER:
            color = BilateralFilter(inputTexture, uv, ipSettings.texelSize, ipSettings.sigma, ipSettings.sigma);
            break;
            
        case OP_MORPHOLOGICAL_GRADIENT:
            color = MorphologicalOperation(inputTexture, uv, ipSettings.texelSize, OP_MORPHOLOGICAL_GRADIENT);
            break;
            
        case OP_NONE:
        default:
            // No operation, pass through
            break;
    }
    
    // Clamp color values
    color = clamp(color, 0.0, 1.0);
    
    // Store result in output image
    outputImage[texelCoord] = color;
}
