// Particle System Compute Shader
// Author: inkbytefo
// Project: AstralEngine
// Description: Particle system simulation using SLANG compute shader

// Constants
static const float PI = 3.14159265359;
static const float TWO_PI = 2.0 * PI;

struct Particle {
    float3 position;
    float lifetime;
    float3 velocity;
    float age;
    float4 color;
    float size;
    int flags; // bit 0: active, bit 1: affected by gravity, etc.
    float padding[2];
};

struct Emitter {
    float3 position;
    float emissionRate;
    float3 direction;
    float spreadAngle;
    float4 startColor;
    float4 endColor;
    float startSize;
    float endSize;
    float startLifetime;
    float endLifetime;
    float startSpeed;
    float endSpeed;
    int particleCount;
    int maxParticles;
    float time;
    float deltaTime;
};

struct SimulationUBO {
    float3 gravity;
    float globalTime;
    float3 wind;
    float deltaTime;
    int simulationMode; // 0: emit, 1: update, 2: both
    int padding[3];
};

struct EmitterUBO {
    Emitter emitter;
};

struct ParticleBuffer {
    Particle particles[];
};

struct IndirectBuffer {
    uint particleCount;
    uint maxParticles;
    uint emitCount;
    uint padding;
};

// Textures
Texture2D noiseTexture : register(t0);
SamplerState linearSampler : register(s0);

// Random number generation
uint RandomUint(uint state)
{
    state = state * 1103515245 + 12345;
    return state;
}

float RandomFloat(uint state)
{
    return float(RandomUint(state)) / float(0xFFFFFFFFu);
}

float3 RandomSphere(uint state)
{
    float theta = RandomFloat(state) * TWO_PI;
    float phi = acos(2.0 * RandomFloat(state + 1) - 1.0);
    float r = pow(RandomFloat(state + 2), 1.0 / 3.0);
    
    float x = r * sin(phi) * cos(theta);
    float y = r * sin(phi) * sin(theta);
    float z = r * cos(phi);
    
    return float3(x, y, z);
}

// Emit new particles
void EmitParticles(uint globalID, uint localID, Emitter emitter, uint currentParticleCount, uint maxParticles, float deltaTime)
{
    // Calculate how many particles to emit this frame
    uint particlesToEmit = uint(emitter.emissionRate * deltaTime);
    
    if (globalID >= particlesToEmit) {
        return;
    }
    
    // Find a dead particle to reuse
    uint particleIndex = currentParticleCount + globalID;
    if (particleIndex >= maxParticles) {
        return;
    }
    
    // Initialize particle
    Particle particle;
    
    // Position
    particle.position = emitter.position;
    
    // Velocity with spread
    float3 randomDir = RandomSphere(globalID * 3 + localID);
    float3 spread = normalize(lerp(emitter.direction, randomDir, emitter.spreadAngle));
    float speed = lerp(emitter.startSpeed, emitter.endSpeed, RandomFloat(globalID * 5 + localID));
    particle.velocity = spread * speed;
    
    // Lifetime
    particle.lifetime = lerp(emitter.startLifetime, emitter.endLifetime, RandomFloat(globalID * 7 + localID));
    particle.age = 0.0;
    
    // Color
    particle.color = lerp(emitter.startColor, emitter.endColor, RandomFloat(globalID * 11 + localID));
    
    // Size
    particle.size = lerp(emitter.startSize, emitter.endSize, RandomFloat(globalID * 13 + localID));
    
    // Flags
    particle.flags = 1; // Active
    
    // Write to buffer (in real implementation, this would be RWStructuredBuffer)
    // particleBuffer.particles[particleIndex] = particle;
}

// Update existing particles
void UpdateParticles(uint globalID, uint localID, SimulationUBO simulation, Texture2D noiseTex)
{
    if (globalID >= simulation.particleCount) {
        return;
    }
    
    // Read particle from buffer (in real implementation, this would be RWStructuredBuffer)
    // Particle particle = particleBuffer.particles[globalID];
    Particle particle; // Placeholder
    
    // Check if particle is active
    if ((particle.flags & 1) == 0) {
        return;
    }
    
    // Update age
    particle.age += simulation.deltaTime;
    
    // Check if particle should die
    if (particle.age >= particle.lifetime) {
        particle.flags = 0; // Deactivate
        // particleBuffer.particles[globalID] = particle;
        return;
    }
    
    // Update position
    particle.position += particle.velocity * simulation.deltaTime;
    
    // Apply forces
    if ((particle.flags & 2) != 0) { // Affected by gravity
        particle.velocity += simulation.gravity * simulation.deltaTime;
    }
    
    // Apply wind
    particle.velocity += simulation.wind * simulation.deltaTime * 0.1;
    
    // Apply drag
    float drag = 0.99;
    particle.velocity *= drag;
    
    // Update color based on age
    float ageFactor = particle.age / particle.lifetime;
    // Color interpolation would be done with emitter colors in a real implementation
    
    // Update size based on age
    // Size interpolation would be done with emitter sizes in a real implementation
    
    // Add some noise for natural movement
    float3 noiseOffset = noiseTex.SampleLevel(linearSampler, float2(globalID * 0.01, simulation.globalTime * 0.1), 0).xyz * 2.0 - 1.0;
    particle.position += noiseOffset * simulation.deltaTime * 0.5;
    
    // Write back to buffer
    // particleBuffer.particles[globalID] = particle;
}

// Collision detection with simple ground plane
void HandleCollisions(uint globalID)
{
    if (globalID >= 1000) { // Placeholder for particle count
        return;
    }
    
    // Read particle from buffer (in real implementation, this would be RWStructuredBuffer)
    // Particle particle = particleBuffer.particles[globalID];
    Particle particle; // Placeholder
    
    // Check if particle is active
    if ((particle.flags & 1) == 0) {
        return;
    }
    
    // Simple ground collision at y = 0
    float groundY = 0.0;
    float restitution = 0.6; // Bounce factor
    
    if (particle.position.y <= groundY) {
        particle.position.y = groundY;
        particle.velocity.y = -particle.velocity.y * restitution;
        
        // Reduce horizontal velocity on bounce (friction)
        particle.velocity.x *= 0.8;
        particle.velocity.z *= 0.8;
        
        // Kill particle if velocity is too low
        if (length(particle.velocity) < 0.1) {
            particle.flags = 0; // Deactivate
        }
    }
    
    // particleBuffer.particles[globalID] = particle;
}

[shader("compute")]
[numthreads(256, 1, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID,
          SimulationUBO simulation : register(b0),
          EmitterUBO emitterData : register(b1))
{
    uint globalID = dispatchThreadID.x;
    uint localID = dispatchThreadID.x % 256; // localID within workgroup
    
    switch (simulation.simulationMode) {
        case 0: // Emit only
            EmitParticles(globalID, localID, emitterData.emitter, 1000, 10000, simulation.deltaTime);
            break;
            
        case 1: // Update only
            UpdateParticles(globalID, localID, simulation, noiseTexture);
            HandleCollisions(globalID);
            break;
            
        case 2: // Both emit and update
            // First half of workgroup emits, second half updates
            if (localID < 128) {
                EmitParticles(globalID, localID, emitterData.emitter, 1000, 10000, simulation.deltaTime);
            } else {
                UpdateParticles(globalID - 128, localID - 128, simulation, noiseTexture);
                HandleCollisions(globalID - 128);
            }
            break;
    }
}
