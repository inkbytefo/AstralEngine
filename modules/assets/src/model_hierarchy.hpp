#pragma once
// Shared by the glTF loader and the cooked-model reader: a model's nodes must form a forest.
#include <astral/assets/model.hpp>

#include <string>
#include <vector>

namespace astral {

/// True when every child index is in range, no node has two parents and the parent links
/// contain no cycle. On failure `error` (if not already set) names the first offending node.
inline bool validate_hierarchy(const std::vector<ModelNode>& nodes, std::string& error) {
    auto fail = [&](std::string message) {
        if (error.empty()) error = std::move(message);
        return false;
    };
    constexpr std::uint32_t none = 0xFFFFFFFFu;
    std::vector<std::uint32_t> parent(nodes.size(), none);
    for (std::uint32_t i = 0; i < nodes.size(); ++i) {
        for (std::uint32_t c : nodes[i].children) {
            if (c >= nodes.size() || c == i) return fail("node " + std::to_string(i) + " has an invalid child");
            if (parent[c] != none) return fail("node " + std::to_string(c) + " has more than one parent");
            parent[c] = i;
        }
    }
    // With one parent each, a cycle is a parent chain that never reaches a root.
    std::vector<std::uint8_t> state(nodes.size(), 0); // 0 unvisited, 1 on the current chain, 2 known good
    for (std::uint32_t start = 0; start < nodes.size(); ++start) {
        std::vector<std::uint32_t> chain;
        std::uint32_t n = start;
        while (n != none && state[n] == 0) {
            state[n] = 1;
            chain.push_back(n);
            n = parent[n];
        }
        if (n != none && state[n] == 1) return fail("node " + std::to_string(n) + " is part of a cycle");
        for (std::uint32_t c : chain) state[c] = 2;
    }
    return true;
}

} // namespace astral
