#include "vk_device.hpp"

#include <vector>

namespace astral::gpu {

namespace {
VkCommandBuffer native(void* p) { return static_cast<VkCommandBuffer>(p); }
} // namespace

QueryPoolHandle Device::create_timestamp_pool(std::uint32_t count) {
    auto& s = *impl_;
    if (!s.timestamps || count == 0) {
        s.log_error("create_timestamp_pool: timestamps are not supported (or count is 0)");
        return {};
    }
    VkQueryPoolCreateInfo info{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
    info.queryType = VK_QUERY_TYPE_TIMESTAMP;
    info.queryCount = count;
    QueryRecord r;
    r.count = count;
    if (vkCreateQueryPool(s.device, &info, nullptr, &r.pool) != VK_SUCCESS) {
        s.log_error("create_timestamp_pool: vkCreateQueryPool failed");
        return {};
    }
    vkResetQueryPool(s.device, r.pool, 0, count); // host reset (Vulkan 1.2): readable as "not available"
    return s.queries.create(r);
}

bool Device::read_timestamps(QueryPoolHandle pool, std::uint32_t first, std::uint32_t count, std::uint64_t* out_ns) {
    auto& s = *impl_;
    const QueryRecord* r = s.queries.get(pool);
    if (!r || !out_ns || count == 0 || first + count > r->count) return false;
    std::vector<std::uint64_t> raw(std::size_t{count} * 2); // value, availability
    const VkResult result = vkGetQueryPoolResults(s.device, r->pool, first, count, raw.size() * sizeof(std::uint64_t), raw.data(),
                                                  2 * sizeof(std::uint64_t), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);
    if (result != VK_SUCCESS && result != VK_NOT_READY) return false;
    for (std::uint32_t i = 0; i < count; ++i) {
        if (raw[std::size_t{i} * 2 + 1] == 0) return false;
    }
    for (std::uint32_t i = 0; i < count; ++i) {
        out_ns[i] = static_cast<std::uint64_t>(static_cast<double>(raw[std::size_t{i} * 2] & s.timestamp_mask) * s.timestamp_period);
    }
    return true;
}

void Device::destroy(QueryPoolHandle pool) {
    auto& s = *impl_;
    const QueryRecord* r = s.queries.get(pool);
    if (!r) return;
    const VkQueryPool native_pool = r->pool;
    s.queries.destroy(pool);
    s.retire([&s, native_pool] { vkDestroyQueryPool(s.device, native_pool, nullptr); });
}

void CommandList::reset_queries(QueryPoolHandle pool, std::uint32_t first, std::uint32_t count) {
    if (!usable()) return;
    auto& s = device_->impl();
    const QueryRecord* r = s.queries.get(pool);
    if (!r || first + count > r->count) return s.log_error("reset_queries: invalid pool or range");
    if (rendering_) return s.log_error("reset_queries: not allowed while rendering");
    vkCmdResetQueryPool(native(native_), r->pool, first, count);
}

void CommandList::write_timestamp(QueryPoolHandle pool, std::uint32_t index) {
    if (!usable()) return;
    auto& s = device_->impl();
    const QueryRecord* r = s.queries.get(pool);
    if (!r || index >= r->count) return s.log_error("write_timestamp: invalid pool or index");
    vkCmdWriteTimestamp2(native(native_), VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, r->pool, index);
}

} // namespace astral::gpu
