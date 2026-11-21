// Astral bindless conventions — include from every shader:
//   #include <astral/bindless.glsl>
//
// set 0, binding 0: sampled textures   (index = gpu::Device::bindless_index(texture))
// set 0, binding 1: storage textures   (same index space, only for Storage-usage textures)
// set 0, binding 2: samplers           (gpu::Sampler enum value)
// Buffers are reached through buffer device addresses (gpu::Device::buffer_address)
// passed in push constants — declare them with ASTRAL_BUFFER below.
#ifndef ASTRAL_BINDLESS_GLSL
#define ASTRAL_BINDLESS_GLSL

#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

layout(set = 0, binding = 0) uniform texture2D astral_textures[];
layout(set = 0, binding = 1, rgba8) uniform image2D astral_storage_images[];
layout(set = 0, binding = 2) uniform sampler astral_samplers[];

#define ASTRAL_SAMPLER_LINEAR_CLAMP 0
#define ASTRAL_SAMPLER_LINEAR_REPEAT 1
#define ASTRAL_SAMPLER_NEAREST_CLAMP 2

// ASTRAL_BUFFER(Name, Type) declares a pointer type `Name` with `Type data[]`.
#define ASTRAL_BUFFER(Name, Type) \
    layout(buffer_reference, scalar, buffer_reference_align = 4) buffer Name { Type data[]; }

vec4 astral_sample(uint texture_index, uint sampler_index, vec2 uv) {
    return texture(sampler2D(astral_textures[nonuniformEXT(texture_index)],
                             astral_samplers[sampler_index]), uv);
}

#endif
