#pragma once

#include "Astral/Core/Threading/JobSystem.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>
#include <atomic>
#include <chrono>
#include <exception>
#include <mutex>

namespace Astral {

/// Subsystem ve kare gorevlerinin Yonlu Dongusuz Cizge (DAG) tabanli planlayicisi.
class TaskGraph {
public:
    TaskGraph() = default;
    ~TaskGraph() = default;

    TaskGraph(const TaskGraph&) = delete;
    TaskGraph& operator=(const TaskGraph&) = delete;

    /// Grafa yeni bir is dugumu ve calismadan once tamamlanmasi gereken bagimliliklarini ekler.
    /// Yinelenen isim veya kendine bagimlilik durumunda std::invalid_argument firlatir.
    void AddTask(
        std::string name,
        std::function<void()> task,
        std::vector<std::string> dependencies = {}
    );

    /// Grafindaki bagimliliklari cozumler ve dogrular.
    /// Eksik bagimlilik veya cevrimsel dongu (cycle) durumunda std::runtime_error firlatir.
    void Validate();

    /// Grafi JobSystem uzerinde asenkron olarak baslatir. Bagimsiz dugumler aninda paralel calisir.
    void Execute(JobSystem& jobSystem);

    /// Grafindaki tum dugumler tamamlanana kadar Work-Helping ile bekler.
    /// Belirtilen sure icinde tamamlanirsa true, zaman asimina ugrarsa false doner.
    /// Eger dugumlerden herhangi biri istisna firlattiysa bu metod icinde tekrar firlatilir.
    bool Wait(JobSystem& jobSystem, std::chrono::milliseconds timeout = std::chrono::milliseconds(10000));

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
        std::atomic<bool> cancelled{false};
    };

    void BuildDependencies();
    void DispatchNode(size_t nodeIdx, JobSystem& jobSystem);
    void CancelDependents(size_t nodeIdx);

    std::vector<std::unique_ptr<TaskNode>> m_Nodes;
    std::unordered_map<std::string, size_t> m_NameToIndex;
    JobHandle m_GraphHandle;
    std::exception_ptr m_FirstException = nullptr;
    std::mutex m_ExceptionMutex;
    std::atomic<bool> m_HasFailed{false};
    bool m_Built = false;
};

} // namespace Astral
