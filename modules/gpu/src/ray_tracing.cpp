// Acceleration structures (VK_KHR_acceleration_structure) for ray queries in any shader.
#include "vk_device.hpp"

#include <cstring>

namespace astral::gpu {

namespace {

VkCommandBuffer native(void* p) { return static_cast<VkCommandBuffer>(p); }

std::uint64_t as_device_address(Device::Impl& s, VkAccelerationStructureKHR as) {
    VkAccelerationStructureDeviceAddressInfoKHR info{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR};
    info.accelerationStructure = as;
    return s.as_address(s.device, &info);
}

/// Allocates the structure's backing buffer and the structure itself.
bool create_structure(Device::Impl& s, VkAccelerationStructureTypeKHR type, std::uint64_t size, AccelRecord& out) {
    if (!create_buffer_raw_usage(s, size, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR,
                                 MemoryUsage::GpuOnly, 256, out.storage)) {
        return false;
    }
    VkAccelerationStructureCreateInfoKHR info{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
    info.buffer = out.storage.buffer;
    info.size = size;
    info.type = type;
    if (s.create_as(s.device, &info, nullptr, &out.handle) != VK_SUCCESS) {
        destroy_buffer_record(s, out.storage);
        return false;
    }
    out.address = as_device_address(s, out.handle);
    return true;
}

VkAccelerationStructureGeometryKHR instances_geometry(std::uint64_t address) {
    VkAccelerationStructureGeometryKHR g{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
    g.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    g.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    g.geometry.instances.arrayOfPointers = VK_FALSE;
    g.geometry.instances.data.deviceAddress = address;
    return g;
}

void as_barrier(VkCommandBuffer cmd, VkPipelineStageFlags2 src_stage, VkAccessFlags2 src_access,
                VkPipelineStageFlags2 dst_stage, VkAccessFlags2 dst_access) {
    VkMemoryBarrier2 b{VK_STRUCTURE_TYPE_MEMORY_BARRIER_2};
    b.srcStageMask = src_stage;
    b.srcAccessMask = src_access;
    b.dstStageMask = dst_stage;
    b.dstAccessMask = dst_access;
    VkDependencyInfo dep{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    dep.memoryBarrierCount = 1;
    dep.pMemoryBarriers = &b;
    vkCmdPipelineBarrier2(cmd, &dep);
}

constexpr VkPipelineStageFlags2 shader_stages =
    VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;

} // namespace

void destroy_accel_record(Device::Impl& s, const AccelRecord& r) {
    if (r.handle) s.destroy_as(s.device, r.handle, nullptr);
    if (r.storage.buffer) destroy_buffer_record(s, r.storage);
    if (r.scratch.buffer) destroy_buffer_record(s, r.scratch);
}

AccelerationStructureHandle Device::create_blas(const BlasDesc& desc) {
    auto& s = *impl_;
    if (!s.ray_tracing) {
        s.log_error("create_blas: ray tracing is not supported on this device");
        return {};
    }
    const BufferRecord* v = s.buffers.get(desc.vertices);
    const BufferRecord* i = s.buffers.get(desc.indices);
    if (!v || !i || desc.vertex_count == 0 || desc.index_count < 3 || desc.index_count % 3 != 0 ||
        desc.vertex_stride < 12 ||
        std::uint64_t{desc.vertex_count} * desc.vertex_stride > v->size ||
        std::uint64_t{desc.index_count} * 4 > i->size) {
        s.log_error("create_blas: invalid geometry (buffers, counts or stride)");
        return {};
    }
    if (!s.can_run_immediate("create_blas")) return {};

    VkAccelerationStructureGeometryKHR geometry{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
    geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    auto& tri = geometry.geometry.triangles;
    tri.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
    tri.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    tri.vertexData.deviceAddress = v->address;
    tri.vertexStride = desc.vertex_stride;
    tri.maxVertex = desc.vertex_count - 1;
    tri.indexType = VK_INDEX_TYPE_UINT32;
    tri.indexData.deviceAddress = i->address;

    VkAccelerationStructureBuildGeometryInfoKHR build{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
    build.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    build.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    build.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    build.geometryCount = 1;
    build.pGeometries = &geometry;
    const std::uint32_t triangles = desc.index_count / 3;
    VkAccelerationStructureBuildSizesInfoKHR sizes{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
    s.as_build_sizes(s.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &build, &triangles, &sizes);

    AccelRecord record;
    BufferRecord scratch;
    if (!create_structure(s, VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR, sizes.accelerationStructureSize, record) ||
        !create_buffer_raw_usage(s, sizes.buildScratchSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, MemoryUsage::GpuOnly,
                                 s.scratch_alignment, scratch)) {
        destroy_accel_record(s, record);
        s.log_error("create_blas: allocation failed");
        return {};
    }
    build.dstAccelerationStructure = record.handle;
    build.scratchData.deviceAddress = scratch.address;
    VkAccelerationStructureBuildRangeInfoKHR range{triangles, 0, 0, 0};
    const VkAccelerationStructureBuildRangeInfoKHR* ranges = &range;
    s.immediate([&](VkCommandBuffer cmd) { s.cmd_build_as(cmd, 1, &build, &ranges); });
    destroy_buffer_record(s, scratch);
    s.name_object(VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR, reinterpret_cast<std::uint64_t>(record.handle),
                  desc.debug_name);
    return s.accels.create(record);
}

AccelerationStructureHandle Device::create_tlas(const TlasDesc& desc) {
    auto& s = *impl_;
    if (!s.ray_tracing) {
        s.log_error("create_tlas: ray tracing is not supported on this device");
        return {};
    }
    if (desc.max_instances == 0) {
        s.log_error("create_tlas: max_instances must be > 0");
        return {};
    }
    const VkAccelerationStructureGeometryKHR geometry = instances_geometry(0);
    VkAccelerationStructureBuildGeometryInfoKHR build{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
    build.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    build.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    build.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    build.geometryCount = 1;
    build.pGeometries = &geometry;
    VkAccelerationStructureBuildSizesInfoKHR sizes{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
    s.as_build_sizes(s.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &build, &desc.max_instances, &sizes);

    AccelRecord record;
    record.max_instances = desc.max_instances;
    if (!create_structure(s, VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR, sizes.accelerationStructureSize, record) ||
        !create_buffer_raw_usage(s, sizes.buildScratchSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, MemoryUsage::GpuOnly,
                                 s.scratch_alignment, record.scratch)) {
        destroy_accel_record(s, record);
        s.log_error("create_tlas: allocation failed");
        return {};
    }
    s.name_object(VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR, reinterpret_cast<std::uint64_t>(record.handle),
                  desc.debug_name);
    return s.accels.create(record);
}

std::uint64_t Device::acceleration_structure_address(AccelerationStructureHandle as) const noexcept {
    const AccelRecord* r = impl_->accels.get(as);
    return r ? r->address : 0;
}

void Device::destroy(AccelerationStructureHandle as) {
    auto& s = *impl_;
    const AccelRecord* r = s.accels.get(as);
    if (!r) return;
    const AccelRecord copy = *r;
    s.accels.destroy(as);
    s.retire([&s, copy] { destroy_accel_record(s, copy); });
}

void CommandList::build_tlas(AccelerationStructureHandle tlas, std::span<const TlasInstance> instances) {
    if (!usable()) return;
    auto& s = device_->impl();
    const AccelRecord* t = s.accels.get(tlas);
    if (!t || t->max_instances == 0 || instances.size() > t->max_instances) {
        s.log_error("build_tlas: invalid TLAS or more instances than its max_instances");
        return;
    }
    // Instances go through a transient host-visible buffer, freed once this list completed.
    const std::uint64_t bytes = std::max<std::uint64_t>(1, instances.size()) * sizeof(VkAccelerationStructureInstanceKHR);
    BufferRecord upload;
    if (!create_buffer_raw_usage(s, bytes, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
                                 MemoryUsage::CpuToGpu, 16, upload)) {
        s.log_error("build_tlas: instance upload allocation failed");
        return;
    }
    auto* out = static_cast<VkAccelerationStructureInstanceKHR*>(upload.mapped);
    std::uint32_t count = 0;
    for (const TlasInstance& in : instances) {
        const AccelRecord* blas = s.accels.get(in.blas);
        if (!blas || blas->max_instances != 0) continue; // dead or not a BLAS: skipped
        VkAccelerationStructureInstanceKHR& vk = out[count++];
        std::memcpy(&vk.transform, in.transform.data(), sizeof(vk.transform));
        vk.instanceCustomIndex = in.custom_index & 0xFFFFFFu;
        vk.mask = in.mask;
        vk.instanceShaderBindingTableRecordOffset = 0;
        vk.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        vk.accelerationStructureReference = blas->address;
    }
    vmaFlushAllocation(s.allocator, upload.allocation, 0, bytes);

    const VkAccelerationStructureGeometryKHR geometry = instances_geometry(upload.address);
    VkAccelerationStructureBuildGeometryInfoKHR build{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
    build.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    build.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    build.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    build.geometryCount = 1;
    build.pGeometries = &geometry;
    build.dstAccelerationStructure = t->handle;
    build.scratchData.deviceAddress = t->scratch.address;
    VkAccelerationStructureBuildRangeInfoKHR range{count, 0, 0, 0};
    const VkAccelerationStructureBuildRangeInfoKHR* ranges = &range;

    VkCommandBuffer cmd = native(native_);
    // Earlier ray queries (previous frames on this queue) and builds must finish before we overwrite.
    as_barrier(cmd, shader_stages | VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
               VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
               VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
               VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR | VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR);
    s.cmd_build_as(cmd, 1, &build, &ranges);
    as_barrier(cmd, VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
               VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR, shader_stages,
               VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR);
    s.retire([&s, upload] { destroy_buffer_record(s, upload); });
}

} // namespace astral::gpu
