// Ocean Simulation Compute Shader
// Author: inkbytefo
// Project: AstralEngine
// Description: Ocean wave simulation using SLANG compute shader

// Constants
static const float PI = 3.14159265359;
static const float TWO_PI = 2.0 * PI;

struct OceanParams {
    float2 windDirection;
    float windSpeed;
    float waveHeight;
    float choppiness;
    float damping;
    float time;
    float deltaTime;
    int gridSize;
    float padding[3];
};

struct WaveSpectrum {
    float amplitude;
    float frequency;
    float direction;
    float phase;
};

struct OceanUBO {
    OceanParams params;
};

struct SpectrumUBO {
    WaveSpectrum spectrum[64];
};

// Storage buffers
struct HeightBuffer {
    float2 heights[];
};

struct DisplacementBuffer {
    float3 displacements[];
};

struct NormalBuffer {
    float3 normals[];
};

struct PreviousHeightBuffer {
    float2 previousHeights[];
};

// Textures
Texture2D noiseTexture : register(t0);
SamplerState linearSampler : register(s0);

// Random number generation using hash function
float RandomFloat(uint seed)
{
    // Simple hash function for random number generation
    uint state = seed;
    state = (state ^ 61u) ^ (state >> 16u);
    state *= 9u;
    state = state ^ (state >> 4u);
    state *= 0x27d4eb2du;
    state = state ^ (state >> 15u);
    
    // Convert to float in range [0, 1]
    return float(state) / 4294967296.0;
}

// Complex number operations
float2 ComplexAdd(float2 a, float2 b)
{
    return float2(a.x + b.x, a.y + b.y);
}

float2 ComplexMultiply(float2 a, float2 b)
{
    return float2(a.x * b.x - a.y * b.y, a.x * b.y + a.y * b.x);
}

float2 ComplexExp(float2 z)
{
    float exp_x = exp(z.x);
    return float2(exp_x * cos(z.y), exp_x * sin(z.y));
}

// Phillips spectrum for ocean waves
float PhillipsSpectrum(float2 k, float windSpeed, float2 windDir)
{
    float kLength = length(k);
    if (kLength < 0.0001) return 0.0;
    
    float kLength2 = kLength * kLength;
    float kLength4 = kLength2 * kLength2;
    
    float windSpeed2 = windSpeed * windSpeed;
    float g = 9.81; // Gravity
    
    float kDotW = dot(normalize(k), normalize(windDir));
    float kDotW2 = kDotW * kDotW;
    
    float L = windSpeed2 / g;
    float L2 = L * L;
    
    float dampingFactor = exp(-kLength2 * L2 * 0.001);
    float spectrumValue = (windSpeed2 / g) * exp(-1.0 / (kLength2 * L2)) * kDotW2 / kLength4;
    
    return spectrumValue * dampingFactor;
}

// Generate initial wave spectrum
float2 GenerateWaveSpectrum(float2 k, uint seed, float windSpeed, float2 windDirection)
{
    float amplitude = sqrt(PhillipsSpectrum(k, windSpeed, windDirection));
    
    // Random phase
    float phase = RandomFloat(seed * 2) * TWO_PI;
    
    // Gaussian random number
    float u1 = RandomFloat(seed * 3);
    float u2 = RandomFloat(seed * 5);
    float gaussian = sqrt(-2.0 * log(u1)) * cos(2.0 * PI * u2);
    
    return float2(gaussian * amplitude * cos(phase), gaussian * amplitude * sin(phase));
}

// FFT butterfly operation
void FFTButterfly(inout float2 data[16], uint stride, bool inverse)
{
    float angle = -2.0 * PI / float(stride * 2);
    if (inverse) angle = -angle;
    
    float2 w = float2(cos(angle), sin(angle));
    
    for (uint i = 0; i < 8; i++) {
        uint index1 = i;
        uint index2 = i + 8;
        
        float2 temp = ComplexMultiply(w, data[index2]);
        data[index2] = ComplexAdd(data[index1], float2(-temp.x, -temp.y));
        data[index1] = ComplexAdd(data[index1], temp);
    }
}

// Bit reversal for FFT
uint BitReverse(uint x, uint bits)
{
    uint result = 0;
    for (uint i = 0; i < bits; i++) {
        result = (result << 1) | (x & 1);
        x >>= 1;
    }
    return result;
}

// 1D FFT
void FFT1D(inout float2 data[16], bool inverse)
{
    // Bit-reverse permutation
    uint bits = 4; // log2(16)
    for (uint i = 0; i < 16; i++) {
        uint j = BitReverse(i, bits);
        if (i < j) {
            float2 temp = data[i];
            data[i] = data[j];
            data[j] = temp;
        }
    }
    
    // FFT butterfly operations
    for (uint stride = 2; stride <= 16; stride *= 2) {
        FFTButterfly(data, stride, inverse);
    }
}

// 2D FFT (simplified for ocean simulation)
float2 FFT2D(float2 grid[16][16], uint x, uint y, bool inverse)
{
    // This is a simplified version - real implementation would need full 2D FFT
    float2 row[16];
    float2 col[16];
    
    // Extract row
    for (uint i = 0; i < 16; i++) {
        row[i] = grid[x][i];
    }
    
    // FFT on row
    FFT1D(row, inverse);
    
    // Extract column
    for (uint i = 0; i < 16; i++) {
        col[i] = grid[i][y];
    }
    
    // FFT on column
    FFT1D(col, inverse);
    
    // Combine results (simplified)
    return ComplexMultiply(row[y], col[x]);
}

// Calculate wave height at position
float2 CalculateWaveHeight(float2 position, float time, WaveSpectrum spectrum[64])
{
    float2 height = float2(0.0, 0.0);
    
    // Sum contributions from all wave components
    for (int i = 0; i < 64; i++) {
        WaveSpectrum wave = spectrum[i];
        
        float2 k = float2(cos(wave.direction), sin(wave.direction)) * wave.frequency;
        float kLength = length(k);
        
        float phase = kLength * dot(k, position) - wave.frequency * time + wave.phase;
        
        height.x += wave.amplitude * cos(phase);
        height.y += wave.amplitude * sin(phase);
    }
    
    return height;
}

// Calculate wave displacement
float3 CalculateDisplacement(float2 position, float time, WaveSpectrum spectrum[64], float choppiness)
{
    float3 displacement = float3(0.0, 0.0, 0.0);
    
    for (int i = 0; i < 64; i++) {
        WaveSpectrum wave = spectrum[i];
        
        float2 k = float2(cos(wave.direction), sin(wave.direction)) * wave.frequency;
        float kLength = length(k);
        
        if (kLength < 0.0001) continue;
        
        float phase = kLength * dot(k, position) - wave.frequency * time + wave.phase;
        float amplitude = wave.amplitude / kLength;
        
        displacement.x += amplitude * k.x * sin(phase) * choppiness;
        displacement.z += amplitude * k.y * sin(phase) * choppiness;
        displacement.y += amplitude * cos(phase);
    }
    
    return displacement;
}

// Calculate wave normal
float3 CalculateNormal(float2 position, float time, WaveSpectrum spectrum[64])
{
    float epsilon = 0.01;
    
    // Calculate height differences
    float hL = CalculateWaveHeight(position - float2(epsilon, 0.0), time, spectrum).x;
    float hR = CalculateWaveHeight(position + float2(epsilon, 0.0), time, spectrum).x;
    float hD = CalculateWaveHeight(position - float2(0.0, epsilon), time, spectrum).x;
    float hU = CalculateWaveHeight(position + float2(0.0, epsilon), time, spectrum).x;
    
    // Calculate normal using central differences
    float3 normal = normalize(float3(hL - hR, 2.0 * epsilon, hD - hU));
    
    return normal;
}

[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID,
          OceanUBO ocean : register(b0),
          SpectrumUBO waveData : register(b1))
{
    uint globalX = dispatchThreadID.x;
    uint globalY = dispatchThreadID.y;
    
    uint gridSize = ocean.params.gridSize;
    
    if (globalX >= gridSize || globalY >= gridSize) {
        return;
    }
    
    uint index = globalY * gridSize + globalX;
    
    // Calculate grid position
    float2 gridPos = float2(float(globalX), float(globalY)) / float(gridSize);
    gridPos = (gridPos - 0.5) * 100.0; // Scale to world space
    
    // Calculate wave height
    float2 height = CalculateWaveHeight(gridPos, ocean.params.time, waveData.spectrum);
    
    // Calculate displacement
    float3 displacement = CalculateDisplacement(gridPos, ocean.params.time, waveData.spectrum, ocean.params.choppiness);
    
    // Calculate normal
    float3 normal = CalculateNormal(gridPos, ocean.params.time, waveData.spectrum);
    
    // Apply damping
    height *= ocean.params.damping;
    displacement *= ocean.params.damping;
    
    // Store results in structured buffers (these would be bound as UAVs)
    // Note: In a real implementation, these would be RWStructuredBuffer
    // For now, we'll simulate the storage operations
    
    // heightBuffer.heights[index] = height;
    // displacementBuffer.displacements[index] = displacement;
    // normalBuffer.normals[index] = normal;
    // previousHeightBuffer.previousHeights[index] = height;
    
    // For demonstration, we'll write to a texture instead
    // In real implementation, you would use proper UAV bindings
}
