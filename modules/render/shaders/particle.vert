#version 460
#include <astral/particle_state.glsl>
layout(push_constant, scalar) uniform Push {
    FrameRef frame;
    vec4 position_age, drift_duration, color_size;
    uint seed, fog, sdf_depth, material;
    uint mesh_depth;
    float soft_distance_m;
    uint64_t states;
} pc;
layout(location = 0) out vec2 corner_out;
layout(location = 1) out float fade_out;
layout(location = 2) out vec3 world_out;

// ASTRAL_PARTICLE_BODY

void main() {
    const vec2 corners[6] = vec2[](vec2(-1,-1), vec2(1,-1), vec2(-1,1), vec2(-1,1), vec2(1,-1), vec2(1,1));
    uint key = uint(gl_InstanceIndex) * 13u + pc.seed * 7919u;
    float delay = pc.drift_duration.w * 0.24 * random_unit(key + 3u);
    float age = max(0.0, pc.position_age.w - delay);
    world_out = pc.states != uint64_t(0) ? ParticleStates(pc.states).data[gl_InstanceIndex].position.xyz :
        pc.position_age.xyz + (particle_velocity(key) + pc.drift_duration.xyz) * age + vec3(0, -1.3 * age * age, 0);
    fade_out = pc.position_age.w < delay ? 0.0 : 1.0 - smoothstep(0.04, pc.drift_duration.w, pc.position_age.w);
    Frame f = pc.frame.f;
    float size_m = pc.color_size.w * (0.35 + random_unit(key + 4u));
#ifdef ASTRAL_PARTICLE_MOTION
    astral_params = AstralParams(uint64_t(f.custom.x) | (uint64_t(f.custom.y) << 32));
    astral_base = Materials(f.scene_materials).data[pc.material].custom.x;
    WORLD_POSITION = world_out;
    ORIGIN = pc.position_age.xyz;
    WIND = pc.drift_duration.xyz;
    AGE = age;
    LIFETIME = pc.drift_duration.w;
    RANDOM = random_unit(key + 5u);
    SIZE = size_m;
    motion();
    world_out = WORLD_POSITION;
    size_m = max(0.0, SIZE);
#endif
    mat4 vp = f.unjittered_view_projection;
    vec2 scale_xy = vec2(length(vec3(vp[0].x, vp[1].x, vp[2].x)),
                         length(vec3(vp[0].y, vp[1].y, vp[2].y)));
    // Depth testing uses the same jittered camera as the depth attachment.
    gl_Position = f.view_projection * vec4(world_out, 1);
    corner_out = corners[gl_VertexIndex % 6];
    gl_Position.xy += corner_out * scale_xy * size_m;
}
