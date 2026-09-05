#include "Astral/Core/Threading/TaskGraph.hpp"
#include <iostream>
#include <stdexcept>
#include <queue>
#include <algorithm>

namespace Astral {

void TaskGraph::AddTask(
    std::string name,
    std::function<void()> task,
    std::vector<std::string> dependencies
) {
    if (m_NameToIndex.find(name) != m_NameToIndex.end()) {
        throw std::invalid_argument("TaskGraph: Yinelenen gorev ismi: '" + name + "'!");
    }

    for (const auto& dep : dependencies) {
        if (dep == name) {
            throw std::invalid_argument("TaskGraph: Kendine bagimlilik tespit edildi: '" + name + "'!");
        }
    }

    size_t idx = m_Nodes.size();
    m_NameToIndex[name] = idx;

    auto node = std::make_unique<TaskNode>();
    node->name = std::move(name);
    node->task = std::move(task);
    node->dependencies = std::move(dependencies);
    node->initialDependencies = static_cast<uint32_t>(node->dependencies.size());
    node->pendingDependencies.store(node->initialDependencies, std::memory_order_relaxed);
    node->cancelled.store(false, std::memory_order_relaxed);

    m_Nodes.push_back(std::move(node));
    m_Built = false;
}

void TaskGraph::Validate() {
    BuildDependencies();
}

void TaskGraph::BuildDependencies() {
    if (m_Built) return;

    for (auto& node : m_Nodes) {
        node->dependents.clear();
    }

    // 1. Bagimliliklarin varligini dogrula ve dependents listesini olustur
    for (size_t i = 0; i < m_Nodes.size(); ++i) {
        for (const auto& depName : m_Nodes[i]->dependencies) {
            auto it = m_NameToIndex.find(depName);
            if (it == m_NameToIndex.end()) {
                throw std::runtime_error("TaskGraph: Eksik bagimlilik: '" + depName + "' dugumu '" + m_Nodes[i]->name + "' tarafindan istendi fakat cizgede bulunamadi!");
            }
            m_Nodes[it->second]->dependents.push_back(i);
        }
    }

    // 2. Kahn's Algoritmasi ile Cevrimsel Dongu (Cycle) Tespiti
    std::vector<uint32_t> inDegree(m_Nodes.size(), 0);
    for (size_t i = 0; i < m_Nodes.size(); ++i) {
        inDegree[i] = m_Nodes[i]->initialDependencies;
    }

    std::vector<size_t> zeroDegreeQueue;
    zeroDegreeQueue.reserve(m_Nodes.size());
    for (size_t i = 0; i < m_Nodes.size(); ++i) {
        if (inDegree[i] == 0) {
            zeroDegreeQueue.push_back(i);
        }
    }

    size_t visitedCount = 0;
    size_t head = 0;
    while (head < zeroDegreeQueue.size()) {
        size_t u = zeroDegreeQueue[head++];
        visitedCount++;

        for (size_t v : m_Nodes[u]->dependents) {
            if (--inDegree[v] == 0) {
                zeroDegreeQueue.push_back(v);
            }
        }
    }

    if (visitedCount != m_Nodes.size()) {
        throw std::runtime_error("TaskGraph: Cizgede cevrimsel bagimlilik (cycle) tespit edildi! Dugumler sirali calistirilamaz.");
    }

    m_Built = true;
}

void TaskGraph::Execute(JobSystem& jobSystem) {
    if (m_Nodes.empty()) return;

    BuildDependencies();
    Reset();

    {
        std::lock_guard<std::mutex> lock(m_ExceptionMutex);
        m_FirstException = nullptr;
    }
    m_HasFailed.store(false, std::memory_order_release);

    m_GraphHandle = std::make_shared<JobCounter>();
    m_GraphHandle->count.store(static_cast<uint32_t>(m_Nodes.size()), std::memory_order_release);

    for (size_t i = 0; i < m_Nodes.size(); ++i) {
        if (m_Nodes[i]->initialDependencies == 0) {
            DispatchNode(i, jobSystem);
        }
    }
}

void TaskGraph::CancelDependents(size_t nodeIdx) {
    for (size_t depIdx : m_Nodes[nodeIdx]->dependents) {
        auto& depNode = *m_Nodes[depIdx];
        bool expected = false;
        if (depNode.cancelled.compare_exchange_strong(expected, true)) {
            m_GraphHandle->count.fetch_sub(1, std::memory_order_acq_rel);
            CancelDependents(depIdx);
        }
    }
}

void TaskGraph::DispatchNode(size_t nodeIdx, JobSystem& jobSystem) {
    jobSystem.PushJobInternal([this, nodeIdx, &jobSystem]() {
        auto& node = *m_Nodes[nodeIdx];
        if (node.cancelled.load(std::memory_order_acquire)) {
            return;
        }

        bool taskSucceeded = false;
        if (!m_HasFailed.load(std::memory_order_acquire)) {
            try {
                if (node.task) {
                    node.task();
                }
                taskSucceeded = true;
            } catch (...) {
                std::lock_guard<std::mutex> lock(m_ExceptionMutex);
                if (!m_FirstException) {
                    m_FirstException = std::current_exception();
                }
                m_HasFailed.store(true, std::memory_order_release);
            }
        }

        if (taskSucceeded && !m_HasFailed.load(std::memory_order_acquire)) {
            // Bagimli dugumleri tetikle
            for (size_t depIdx : node.dependents) {
                auto& depNode = *m_Nodes[depIdx];
                uint32_t prev = depNode.pendingDependencies.fetch_sub(1, std::memory_order_acq_rel);
                if (prev == 1) {
                    DispatchNode(depIdx, jobSystem);
                }
            }
        } else if (!taskSucceeded) {
            // Gorev basarisiz oldu; calisamayacak alt bagimliliklari iptal et ki sayac 0'a ulasabilsin
            CancelDependents(nodeIdx);
        }

        m_GraphHandle->count.fetch_sub(1, std::memory_order_acq_rel);
    });
}

bool TaskGraph::Wait(JobSystem& jobSystem, std::chrono::milliseconds timeout) {
    bool completed = true;
    if (m_GraphHandle) {
        completed = jobSystem.Wait(m_GraphHandle, timeout);
    }

    std::exception_ptr ex = nullptr;
    {
        std::lock_guard<std::mutex> lock(m_ExceptionMutex);
        ex = m_FirstException;
    }

    if (ex) {
        std::rethrow_exception(ex);
    }

    return completed;
}

void TaskGraph::Reset() {
    for (auto& node : m_Nodes) {
        node->pendingDependencies.store(node->initialDependencies, std::memory_order_release);
        node->cancelled.store(false, std::memory_order_release);
    }
    {
        std::lock_guard<std::mutex> lock(m_ExceptionMutex);
        m_FirstException = nullptr;
    }
    m_HasFailed.store(false, std::memory_order_release);
}

void TaskGraph::Clear() {
    m_Nodes.clear();
    m_NameToIndex.clear();
    m_GraphHandle.reset();
    {
        std::lock_guard<std::mutex> lock(m_ExceptionMutex);
        m_FirstException = nullptr;
    }
    m_HasFailed.store(false, std::memory_order_release);
    m_Built = false;
}

} // namespace Astral
