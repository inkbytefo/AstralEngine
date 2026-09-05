#include "TestFramework.hpp"
#include "Astral/Core/Threading/JobSystem.hpp"
#include "Astral/Core/Threading/TaskGraph.hpp"
#include <iostream>
#include <vector>
#include <atomic>
#include <chrono>
#include <stdexcept>

namespace Astral::Test {

void RunTaskGraphTests() {
    const std::string suite = "TaskGraphSuite";
    std::cout << "  [INFO] TaskGraph DAG planlayici testleri baslatiliyor...\n";

    // -------------------------------------------------------------------------
    // 1. Sirali Bagimlilik Zinciri: A -> B -> C (4 Worker)
    // -------------------------------------------------------------------------
    {
        JobSystem jobSystem;
        jobSystem.Initialize(4);

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
        bool completed = graph.Wait(jobSystem, std::chrono::seconds(5));

        bool orderCorrect = (completed && executionOrder.size() == 3 &&
                             executionOrder[0] == 1 &&
                             executionOrder[1] == 2 &&
                             executionOrder[2] == 3);

        TEST_CHECK_MSG(suite, "SequentialDependencyOrder", orderCorrect,
                       "A -> B -> C bagimlilik siralamasi tam olarak korunmali");

        jobSystem.Shutdown();
    }

    // -------------------------------------------------------------------------
    // 2. Elmas (Diamond) Paralel DAG Grafinda Senkronizasyon (4 Worker)
    //         ┌── TaskB (Fizik) ──────┐
    // TaskA ──┤                       ├── TaskD (Render Extraction)
    // (Girdi) └── TaskC (Ses / AI) ───┘
    // -------------------------------------------------------------------------
    {
        JobSystem jobSystem;
        jobSystem.Initialize(4);

        TaskGraph graph;
        std::atomic<bool> taskA_Done{false};
        std::atomic<bool> taskB_Done{false};
        std::atomic<bool> taskC_Done{false};
        std::atomic<bool> taskD_Done{false};

        std::atomic<bool> b_saw_a{false};
        std::atomic<bool> c_saw_a{false};
        std::atomic<bool> d_saw_bc{false};

        graph.AddTask("TaskA_Input", [&]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(3));
            taskA_Done.store(true, std::memory_order_release);
        });

        graph.AddTask("TaskB_Physics", [&]() {
            if (taskA_Done.load(std::memory_order_acquire)) {
                b_saw_a.store(true, std::memory_order_release);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(3));
            taskB_Done.store(true, std::memory_order_release);
        }, {"TaskA_Input"});

        graph.AddTask("TaskC_Audio", [&]() {
            if (taskA_Done.load(std::memory_order_acquire)) {
                c_saw_a.store(true, std::memory_order_release);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(3));
            taskC_Done.store(true, std::memory_order_release);
        }, {"TaskA_Input"});

        graph.AddTask("TaskD_Extraction", [&]() {
            if (taskB_Done.load(std::memory_order_acquire) && taskC_Done.load(std::memory_order_acquire)) {
                d_saw_bc.store(true, std::memory_order_release);
            }
            taskD_Done.store(true, std::memory_order_release);
        }, {"TaskB_Physics", "TaskC_Audio"});

        graph.Execute(jobSystem);
        bool completed = graph.Wait(jobSystem, std::chrono::seconds(5));

        TEST_CHECK_MSG(suite, "DiamondCompletedWithinTimeout", completed,
                       "Elmas DAG belirtilen zaman asimi suresinde tamamlanmali");
        TEST_CHECK_MSG(suite, "DiamondBranchBDepA", b_saw_a.load(std::memory_order_acquire),
                       "TaskB calistiginda TaskA tamamlanmis olmali");
        TEST_CHECK_MSG(suite, "DiamondBranchCDepA", c_saw_a.load(std::memory_order_acquire),
                       "TaskC calistiginda TaskA tamamlanmis olmali");
        TEST_CHECK_MSG(suite, "DiamondSinkDDepBC", d_saw_bc.load(std::memory_order_acquire),
                       "TaskD calistiginda hem TaskB hem TaskC tamamlanmis olmali");
        TEST_CHECK_MSG(suite, "DiamondAllFinished", taskD_Done.load(std::memory_order_acquire),
                       "Tum elmas grafi basariyla tamamlanmali");

        jobSystem.Shutdown();
    }

    // -------------------------------------------------------------------------
    // 3. Tek Worker ile Calistirma (1 Worker Thread Olcek Testi)
    // -------------------------------------------------------------------------
    {
        JobSystem singleWorkerJS;
        singleWorkerJS.Initialize(1);

        TaskGraph graph;
        std::atomic<uint32_t> stepsRun{0};

        graph.AddTask("Single1", [&stepsRun]() { stepsRun.fetch_add(1); });
        graph.AddTask("Single2", [&stepsRun]() { stepsRun.fetch_add(2); }, {"Single1"});
        graph.AddTask("Single3", [&stepsRun]() { stepsRun.fetch_add(4); }, {"Single2"});

        graph.Execute(singleWorkerJS);
        bool finished = graph.Wait(singleWorkerJS, std::chrono::seconds(5));

        TEST_CHECK_MSG(suite, "SingleWorkerExecution", finished && stepsRun.load() == 7,
                       "Tek worker thread ile TaskGraph sirali bagimliliklari sorunsuz calistirmali");

        singleWorkerJS.Shutdown();
    }

    // -------------------------------------------------------------------------
    // 4. Grafi Sifirlama (Reset) ve Coklu Kare Calistirma
    // -------------------------------------------------------------------------
    {
        JobSystem jobSystem;
        jobSystem.Initialize(4);

        TaskGraph graph;
        std::atomic<uint32_t> runs{0};

        graph.AddTask("Step1", [&runs]() { runs.fetch_add(1, std::memory_order_relaxed); });
        graph.AddTask("Step2", [&runs]() { runs.fetch_add(10, std::memory_order_relaxed); }, {"Step1"});

        // 1. Kare
        graph.Execute(jobSystem);
        graph.Wait(jobSystem, std::chrono::seconds(5));
        TEST_CHECK_MSG(suite, "MultiFrameRun1", runs.load(std::memory_order_acquire) == 11,
                       "Ilk kare calismasi sonucu 11 olmali");

        // 2. Kare (Reset ile tekrar calistirma)
        graph.Reset();
        graph.Execute(jobSystem);
        graph.Wait(jobSystem, std::chrono::seconds(5));
        TEST_CHECK_MSG(suite, "MultiFrameRun2", runs.load(std::memory_order_acquire) == 22,
                       "Ikinci kare calismasi sonucu 22 olmali");

        jobSystem.Shutdown();
    }

    // -------------------------------------------------------------------------
    // 5. 100 Ardisik Calistirma Dongusu (Kilitlenme ve Stres Regresyonu)
    // -------------------------------------------------------------------------
    {
        JobSystem stressJS;
        stressJS.Initialize(4);

        bool allPassed = true;
        for (int i = 0; i < 100; ++i) {
            TaskGraph graph;
            std::atomic<int> counter{0};

            graph.AddTask("A", [&counter]() { counter.fetch_add(1); });
            graph.AddTask("B", [&counter]() { counter.fetch_add(2); }, {"A"});
            graph.AddTask("C", [&counter]() { counter.fetch_add(4); }, {"A"});
            graph.AddTask("D", [&counter]() { counter.fetch_add(8); }, {"B", "C"});

            graph.Execute(stressJS);
            bool ok = graph.Wait(stressJS, std::chrono::seconds(2));
            if (!ok || counter.load() != 15) {
                allPassed = false;
                break;
            }
        }

        TEST_CHECK_MSG(suite, "Stress100ConsecutiveRuns", allPassed,
                       "100 ardisik TaskGraph calistirmasi kilitlenmeden tamamlanmali");

        stressJS.Shutdown();
    }

    // -------------------------------------------------------------------------
    // 6. Gecersiz Cizge Dogrulama (DAG Validation): Eksik Bagimlilik Reddi
    // -------------------------------------------------------------------------
    {
        TaskGraph graph;
        graph.AddTask("TaskA", []() {});
        graph.AddTask("TaskB", []() {}, {"VarOlmayanGorev"});

        bool threwMissingDep = false;
        try {
            graph.Validate();
        } catch (const std::runtime_error& e) {
            threwMissingDep = true;
        }

        TEST_CHECK_MSG(suite, "ValidateMissingDependencyRejected", threwMissingDep,
                       "Var olmayan bir goreve bagimlilik cizge dogrulamasinda reddedilmeli");
    }

    // -------------------------------------------------------------------------
    // 7. Gecersiz Cizge Dogrulama: Yinelenen Isim Reddi
    // -------------------------------------------------------------------------
    {
        TaskGraph graph;
        graph.AddTask("AyniIsim", []() {});

        bool threwDuplicate = false;
        try {
            graph.AddTask("AyniIsim", []() {});
        } catch (const std::invalid_argument& e) {
            threwDuplicate = true;
        }

        TEST_CHECK_MSG(suite, "ValidateDuplicateNameRejected", threwDuplicate,
                       "Yinelenen gorev ismi ekleme aninda invalid_argument ile reddedilmeli");
    }

    // -------------------------------------------------------------------------
    // 8. Gecersiz Cizge Dogrulama: Kendine Bagimlilik Reddi
    // -------------------------------------------------------------------------
    {
        TaskGraph graph;
        bool threwSelfDep = false;
        try {
            graph.AddTask("KendineBagimli", []() {}, {"KendineBagimli"});
        } catch (const std::invalid_argument& e) {
            threwSelfDep = true;
        }

        TEST_CHECK_MSG(suite, "ValidateSelfDependencyRejected", threwSelfDep,
                       "Bir gorevin kendine bagimli olmasi invalid_argument ile reddedilmeli");
    }

    // -------------------------------------------------------------------------
    // 9. Gecersiz Cizge Dogrulama: Cevrim / Dongu (Cycle: A -> B -> A) Reddi
    // -------------------------------------------------------------------------
    {
        TaskGraph graph;
        graph.AddTask("NodeA", []() {}, {"NodeB"});
        graph.AddTask("NodeB", []() {}, {"NodeA"});

        bool threwCycle = false;
        try {
            graph.Validate();
        } catch (const std::runtime_error& e) {
            threwCycle = true;
        }

        TEST_CHECK_MSG(suite, "ValidateCyclicDependencyRejected", threwCycle,
                       "Cizgedeki cevrim/dongu (cycle) Kahn's algoritmasi ile tespit edilip reddedilmeli");
    }

    // -------------------------------------------------------------------------
    // 10. Gorev Ici Istisna Guvenligi (Exception Propagation)
    // -------------------------------------------------------------------------
    {
        JobSystem jobSystem;
        jobSystem.Initialize(4);

        TaskGraph graph;
        graph.AddTask("FailingTask", []() {
            throw std::runtime_error("Gorev icinde beklenen test hatasi!");
        });
        graph.AddTask("DependentOfFailed", []() {}, {"FailingTask"});

        graph.Execute(jobSystem);

        bool exceptionCaughtInWait = false;
        try {
            graph.Wait(jobSystem, std::chrono::seconds(5));
        } catch (const std::runtime_error& e) {
            exceptionCaughtInWait = true;
        }

        TEST_CHECK_MSG(suite, "TaskExceptionPropagatedToWait", exceptionCaughtInWait,
                       "Gorev icinde olusan istisna Wait() tarafindan yakalanip cagirana iletilmeli, asili kalmamali");

        jobSystem.Shutdown();
    }

    // -------------------------------------------------------------------------
    // 11. Zaman Asimi Korumasi (Timeout Handling)
    // -------------------------------------------------------------------------
    {
        JobSystem jobSystem;
        jobSystem.Initialize(2);

        TaskGraph graph;
        graph.AddTask("LongTask", []() {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        });

        graph.Execute(jobSystem);

        // Cok kisa timeout (10 ms) -> false donmeli
        bool timedOut = !graph.Wait(jobSystem, std::chrono::milliseconds(10));
        TEST_CHECK_MSG(suite, "WaitTimeoutTriggered", timedOut,
                       "Gorev suresi timeout'tan uzun ise Wait() false dondurerek asili kalmayi engellemeli");

        // Yeterli sure ile bekle -> true donmeli
        bool completed = graph.Wait(jobSystem, std::chrono::seconds(2));
        TEST_CHECK_MSG(suite, "WaitCompletedAfterTimeout", completed,
                       "Gorev tamamlandiginda sonraki Wait() basariyla tamamlanmali");

        jobSystem.Shutdown();
    }
}

} // namespace Astral::Test
