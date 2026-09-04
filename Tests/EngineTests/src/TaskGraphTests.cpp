#include "TestFramework.hpp"
#include "Astral/Core/Threading/JobSystem.hpp"
#include "Astral/Core/Threading/TaskGraph.hpp"
#include <iostream>
#include <vector>
#include <atomic>
#include <chrono>

namespace Astral::Test {

void RunTaskGraphTests() {
    const std::string suite = "TaskGraphSuite";
    std::cout << "  [INFO] TaskGraph DAG planlayici testleri baslatiliyor...\n";

    JobSystem jobSystem;
    jobSystem.Initialize(4);

    // 1. Sirali Bagimlilik Zinciri: A -> B -> C
    {
        TaskGraph graph;
        std::vector<int> executionOrder;
        std::mutex orderMutex;

        graph.AddTask("TaskA", [&]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            std::lock_guard<std::mutex> lock(orderMutex);
            executionOrder.push_back(1);
        });

        graph.AddTask("TaskB", [&]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            std::lock_guard<std::mutex> lock(orderMutex);
            executionOrder.push_back(2);
        }, {"TaskA"});

        graph.AddTask("TaskC", [&]() {
            std::lock_guard<std::mutex> lock(orderMutex);
            executionOrder.push_back(3);
        }, {"TaskB"});

        graph.Execute(jobSystem);
        graph.Wait(jobSystem);

        bool orderCorrect = (executionOrder.size() == 3 &&
                             executionOrder[0] == 1 &&
                             executionOrder[1] == 2 &&
                             executionOrder[2] == 3);

        TEST_CHECK_MSG(suite, "SequentialDependencyOrder", orderCorrect,
                       "A -> B -> C bagimlilik siralamasi tam olarak korunmali");
    }

    // 2. Elmas (Diamond) Paralel DAG Grafinda Senkronizasyon:
    //         ┌── TaskB (Fizik) ──────┐
    // TaskA ──┤                       ├── TaskD (Render Extraction)
    // (Girdi) └── TaskC (Ses / AI) ───┘
    {
        TaskGraph graph;
        std::atomic<bool> taskA_Done{false};
        std::atomic<bool> taskB_Done{false};
        std::atomic<bool> taskC_Done{false};
        std::atomic<bool> taskD_Done{false};

        std::atomic<bool> b_saw_a{false};
        std::atomic<bool> c_saw_a{false};
        std::atomic<bool> d_saw_bc{false};

        graph.AddTask("TaskA_Input", [&]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            taskA_Done.store(true, std::memory_order_release);
        });

        graph.AddTask("TaskB_Physics", [&]() {
            if (taskA_Done.load(std::memory_order_acquire)) {
                b_saw_a.store(true, std::memory_order_release);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            taskB_Done.store(true, std::memory_order_release);
        }, {"TaskA_Input"});

        graph.AddTask("TaskC_Audio", [&]() {
            if (taskA_Done.load(std::memory_order_acquire)) {
                c_saw_a.store(true, std::memory_order_release);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            taskC_Done.store(true, std::memory_order_release);
        }, {"TaskA_Input"});

        graph.AddTask("TaskD_Extraction", [&]() {
            if (taskB_Done.load(std::memory_order_acquire) && taskC_Done.load(std::memory_order_acquire)) {
                d_saw_bc.store(true, std::memory_order_release);
            }
            taskD_Done.store(true, std::memory_order_release);
        }, {"TaskB_Physics", "TaskC_Audio"});

        graph.Execute(jobSystem);
        graph.Wait(jobSystem);

        TEST_CHECK_MSG(suite, "DiamondBranchBDepA", b_saw_a.load(std::memory_order_acquire),
                       "TaskB calistiginda TaskA tamamlanmis olmali");
        TEST_CHECK_MSG(suite, "DiamondBranchCDepA", c_saw_a.load(std::memory_order_acquire),
                       "TaskC calistiginda TaskA tamamlanmis olmali");
        TEST_CHECK_MSG(suite, "DiamondSinkDDepBC", d_saw_bc.load(std::memory_order_acquire),
                       "TaskD calistiginda hem TaskB hem TaskC tamamlanmis olmali");
        TEST_CHECK_MSG(suite, "DiamondAllFinished", taskD_Done.load(std::memory_order_acquire),
                       "Tum elmas grafi basariyla tamamlanmali");
    }

    // 3. Grafi Sifirlama (Reset) ve Coklu Kare Calistirma
    {
        TaskGraph graph;
        std::atomic<uint32_t> runs{0};

        graph.AddTask("Step1", [&runs]() { runs.fetch_add(1, std::memory_order_relaxed); });
        graph.AddTask("Step2", [&runs]() { runs.fetch_add(10, std::memory_order_relaxed); }, {"Step1"});

        // 1. Kare
        graph.Execute(jobSystem);
        graph.Wait(jobSystem);
        TEST_CHECK_MSG(suite, "MultiFrameRun1", runs.load(std::memory_order_acquire) == 11,
                       "Ilk kare calismasi sonucu 11 olmali");

        // 2. Kare (Reset ile tekrar calistirma)
        graph.Reset();
        graph.Execute(jobSystem);
        graph.Wait(jobSystem);
        TEST_CHECK_MSG(suite, "MultiFrameRun2", runs.load(std::memory_order_acquire) == 22,
                       "Ikinci kare calismasi sonucu 22 olmali");
    }

    jobSystem.Shutdown();
}

} // namespace Astral::Test
