#include "Astral/Core/Threading/TaskGraph.hpp"
#include <iostream>

namespace Astral {

void TaskGraph::AddTask(
    std::string name,
    std::function<void()> task,
    std::vector<std::string> dependencies
) {
    size_t idx = m_Nodes.size();
    m_NameToIndex[name] = idx;

    auto node = std::make_unique<TaskNode>();
    node->name = std::move(name);
    node->task = std::move(task);
    node->dependencies = std::move(dependencies);
    node->initialDependencies = static_cast<uint32_t>(node->dependencies.size());
    node->pendingDependencies.store(node->initialDependencies, std::memory_order_relaxed);

    m_Nodes.push_back(std::move(node));
    m_Built = false;
}

void TaskGraph::BuildDependencies() {
    if (m_Built) return;

    for (auto& node : m_Nodes) {
        node->dependents.clear();
    }

    for (size_t i = 0; i < m_Nodes.size(); ++i) {
        for (const auto& depName : m_Nodes[i]->dependencies) {
            auto it = m_NameToIndex.find(depName);
            if (it != m_NameToIndex.end()) {
                m_Nodes[it->second]->dependents.push_back(i);
            }
        }
    }

    m_Built = true;
}

void TaskGraph::Execute(JobSystem& jobSystem) {
    if (m_Nodes.empty()) return;

    BuildDependencies();
    Reset();

    m_GraphHandle = std::make_shared<JobCounter>();
    m_GraphHandle->count.store(static_cast<uint32_t>(m_Nodes.size()), std::memory_order_release);

    for (size_t i = 0; i < m_Nodes.size(); ++i) {
        if (m_Nodes[i]->initialDependencies == 0) {
            DispatchNode(i, jobSystem);
        }
    }
}

void TaskGraph::DispatchNode(size_t nodeIdx, JobSystem& jobSystem) {
    jobSystem.PushJobInternal([this, nodeIdx, &jobSystem]() {
        auto& node = *m_Nodes[nodeIdx];
        if (node.task) {
            node.task();
        }

        // Bagimli dugumleri tetikle
        for (size_t depIdx : node.dependents) {
            auto& depNode = *m_Nodes[depIdx];
            uint32_t prev = depNode.pendingDependencies.fetch_sub(1, std::memory_order_acq_rel);
            if (prev == 1) {
                // Tum bagimliliklari bitti, calistir
                DispatchNode(depIdx, jobSystem);
            }
        }

        m_GraphHandle->count.fetch_sub(1, std::memory_order_acq_rel);
    });
}

void TaskGraph::Wait(JobSystem& jobSystem) {
    if (m_GraphHandle) {
        jobSystem.Wait(m_GraphHandle);
    }
}

void TaskGraph::Reset() {
    for (auto& node : m_Nodes) {
        node->pendingDependencies.store(node->initialDependencies, std::memory_order_release);
    }
}

void TaskGraph::Clear() {
    m_Nodes.clear();
    m_NameToIndex.clear();
    m_GraphHandle.reset();
    m_Built = false;
}

} // namespace Astral
