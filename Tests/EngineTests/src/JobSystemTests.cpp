#include "TestFramework.hpp"
#include "Astral/Core/Threading/JobSystem.hpp"
#include <iostream>
#include <vector>
#include <atomic>
#include <chrono>

namespace Astral::Test {

void RunJobSystemTests() {
    const std::string suite = "JobSystemSuite";
    std::cout << "  [INFO] C++20 JobSystem testleri baslatiliyor...\n";

    JobSystem jobSystem;
    jobSystem.Initialize(4); // 4 worker thread

    TEST_CHECK_MSG(suite, "JobSystemInitialized", jobSystem.IsInitialized(), "JobSystem basariyla baslatilmis olmali");
    TEST_CHECK_MSG(suite, "JobSystemWorkerCount", jobSystem.GetWorkerCount() == 4, "Worker sayisi 4 olmali");

    // 1. Tekil Gorev Dispatch ve Wait
    {
        std::atomic<bool> jobExecuted{false};
        auto handle = jobSystem.Dispatch([&jobExecuted]() {
            jobExecuted.store(true, std::memory_order_release);
        });

        jobSystem.Wait(handle);
        TEST_CHECK_MSG(suite, "SingleJobExecution", jobExecuted.load(std::memory_order_acquire),
                       "Tekil gorev basariyla calisip tamamlanmali");
        TEST_CHECK_MSG(suite, "SingleJobIsDone", jobSystem.IsDone(handle), "Gorev handle IsDone() true donmeli");
    }

    // 2. Coklu Bagimsiz Gorev ve Atomik Sayac Dogrulamasi
    {
        constexpr size_t JOB_COUNT = 200;
        std::atomic<uint32_t> counter{0};
        std::vector<JobHandle> handles;
        handles.reserve(JOB_COUNT);

        for (size_t i = 0; i < JOB_COUNT; ++i) {
            handles.push_back(jobSystem.Dispatch([&counter]() {
                counter.fetch_add(1, std::memory_order_relaxed);
            }));
        }

        for (const auto& h : handles) {
            jobSystem.Wait(h);
        }

        TEST_CHECK_MSG(suite, "MultipleJobsCounter", counter.load(std::memory_order_acquire) == JOB_COUNT,
                       "200 adet asenkron gorev sayaci tam olarak 200 yapmali");
    }

    // 3. Ebeveyn-Cocuk (Child Jobs) Hiyerarsisi
    {
        constexpr size_t CHILD_COUNT = 50;
        std::atomic<uint32_t> childCounter{0};

        auto parentHandle = std::make_shared<JobCounter>();
        // Parent sayaci 0 baslar, Dispatch(parent, ...) her cagrilista sayaci atomik artirir

        for (size_t i = 0; i < CHILD_COUNT; ++i) {
            jobSystem.Dispatch(parentHandle, [&childCounter]() {
                childCounter.fetch_add(1, std::memory_order_relaxed);
            });
        }

        jobSystem.Wait(parentHandle);
        TEST_CHECK_MSG(suite, "ParentChildHierarchy", childCounter.load(std::memory_order_acquire) == CHILD_COUNT,
                       "Ebeveyn handle altindaki tum 50 alt is tamamlanana kadar Wait() beklemeli");
        TEST_CHECK_MSG(suite, "ParentHandleDone", jobSystem.IsDone(parentHandle), "Ebeveyn handle IsDone() true olmali");
    }

    // 4. ParallelFor ile 100,000 Elemanli Dizi Isleme
    {
        constexpr size_t ARRAY_SIZE = 100000;
        constexpr size_t BATCH_SIZE = 2500;
        std::vector<int> data(ARRAY_SIZE, 0);

        jobSystem.ParallelFor(ARRAY_SIZE, BATCH_SIZE, [&data](size_t i) {
            data[i] = static_cast<int>(i * 3 + 1);
        });

        bool integrityValid = true;
        for (size_t i = 0; i < ARRAY_SIZE; ++i) {
            if (data[i] != static_cast<int>(i * 3 + 1)) {
                integrityValid = false;
                break;
            }
        }

        TEST_CHECK_MSG(suite, "ParallelForIntegrity", integrityValid,
                       "ParallelFor ile 100,000 elemanli veri kumesi veri yarisi olmadan dogru islenmeli");
    }

    // 5. Work-Helping Mekanizmasi
    {
        std::atomic<bool> longJobStarted{false};
        std::atomic<bool> helperJobDone{false};

        auto longJob = jobSystem.Dispatch([&longJobStarted]() {
            longJobStarted.store(true, std::memory_order_release);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        });

        // Kuyruga kisa bir is daha at
        jobSystem.Dispatch([&helperJobDone]() {
            helperJobDone.store(true, std::memory_order_release);
        });

        // Ana thread longJob'u beklerken kuyruktaki diger isi yardimlasarak (work-helping) calistirir
        jobSystem.Wait(longJob);

        TEST_CHECK_MSG(suite, "WorkHelpingCompleted", helperJobDone.load(std::memory_order_acquire),
                       "Ana thread beklemedeyken kuyruktaki isleri work-helping ile calistirmali");
    }

    jobSystem.Shutdown();
    TEST_CHECK_MSG(suite, "JobSystemShutdown", !jobSystem.IsInitialized(), "JobSystem basariyla kapatilmis olmali");
}

} // namespace Astral::Test
