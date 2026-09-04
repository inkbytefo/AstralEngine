#pragma once

#include "Astral/Core/Threading/JobSystem.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>
#include <atomic>

namespace Astral {

/// Subsystem ve kare gorevlerinin Yonlu Dongusuz Cizge (DAG) tabanli planlayicisi.
class TaskGraph {
public:
    TaskGraph() = default;
    ~TaskGraph() = default;

    TaskGraph(const TaskGraph&) = delete;
    TaskGraph& operator=(const TaskGraph&) = delete;

    /// Grafa yeni bir is dugumu ve calismadan once tamamlanmasi gereken bagimliliklarini ekler.
    void AddTask(
        std::string name,
        std::function<void()> task,
        std::vector<std::string> dependencies = {}
    );

    /// Grafi JobSystem uzerinde asenkron olarak baslatir. Bagimsiz dugumler aninda paralel calisir.
    void Execute(JobSystem& jobSystem);

    /// Grafindaki tum dugumler tamamlanana kadar Work-Helping ile bekler.
    void Wait(JobSystem& jobSystem);

    /// Grafi sonraki karede tekrar calistirmak uzere sayaclari sifirlar.
    void Reset();

    /// Tum grafi ve dugumleri temizler.
    void Clear();

    [[nodiscard]] size_t GetTaskCount() const noexcept { return m_Nodes.size(); }

private:
    struct TaskNode {
        std::string name;
        std::function<void()> task;
        std::vector<std::string> dependencies;
        std::vector<size_t> dependents; // Bu dugume bagimli diger dugumler
        std::atomic<uint32_t> pendingDependencies{0};
        uint32_t initialDependencies = 0;
    };

    void BuildDependencies();
    void DispatchNode(size_t nodeIdx, JobSystem& jobSystem);

    std::vector<std::unique_ptr<TaskNode>> m_Nodes;
    std::unordered_map<std::string, size_t> m_NameToIndex;
    JobHandle m_GraphHandle;
    bool m_Built = false;
};

} // namespace Astral
