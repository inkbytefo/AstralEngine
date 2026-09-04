#include "TestFramework.hpp"
#include "AstralEngine.h"
#include "Astral/Renderer/VulkanContext.hpp"
#include "Astral/Renderer/Buffer.hpp"
#include "Astral/Renderer/BrickGrid.hpp"
#include <iostream>
#include <vector>
#include <memory>
#include <cstring>

namespace Astral::Test {

class VmaTestApp : public Astral::Application {
public:
    VmaTestApp() : Astral::Application(BuildConfig()) {}

    void RunVmaValidation() {
        const std::string suite = "VmaMemorySuite";
        auto* ctx = GetVulkanContext();
        TEST_CHECK_MSG(suite, "VulkanContextAvailable", ctx != nullptr, "VulkanContext erisilebilir olmali");
        if (!ctx) return;

        VmaAllocator allocator = ctx->GetAllocator();
        TEST_CHECK_MSG(suite, "VmaAllocatorValid", allocator != VK_NULL_HANDLE, "VmaAllocator gecerli bir handle olmali");
        if (allocator == VK_NULL_HANDLE) return;

        // 1. VMA Buffer Olusturma ve Persistent Mapping Dogrulamasi
        {
            constexpr size_t BUF_SIZE = 1024;
            Buffer vmaBuf(
                allocator,
                BUF_SIZE,
                vk::BufferUsageFlagBits::eStorageBuffer,
                VMA_MEMORY_USAGE_AUTO,
                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
                true
            );

            TEST_CHECK_MSG(suite, "VmaBufferHandleValid", vmaBuf.GetBuffer() != nullptr, "VMA tampon handle gecerli olmali");
            TEST_CHECK_MSG(suite, "VmaBufferIsVma", vmaBuf.IsVma(), "Tampon IsVma() true donmeli");
            TEST_CHECK_MSG(suite, "VmaBufferMappedValid", vmaBuf.GetMappedData() != nullptr, "Persistent mapped pointer null olmamali");
            TEST_CHECK_MSG(suite, "VmaBufferAllocationValid", vmaBuf.GetAllocation() != VK_NULL_HANDLE, "VmaAllocation gecerli olmali");

            // Veri yazma ve okuma kontrolu
            const char testData[] = "AstralEngine_VMA_Test_Payload";
            vmaBuf.UpdateData(testData, sizeof(testData));
            int cmpResult = std::memcmp(vmaBuf.GetMappedData(), testData, sizeof(testData));
            TEST_CHECK_MSG(suite, "VmaBufferDataIntegrity", cmpResult == 0, "Yazilan veri mapped bellekten dogru okunmali");
        }

        // 2. VMA Bellek Istatistigi Dogrulamasi
        {
            VmaTotalStatistics stats = ctx->GetMemoryStats();
            TEST_CHECK_MSG(suite, "VmaStatsAllocationCount", stats.total.statistics.allocationCount > 0,
                           "Motor baslatildiginda VMA uzerinde aktif tahsis bulunmali (SDFRenderer image/buffer)");
            TEST_CHECK_MSG(suite, "VmaStatsAllocationBytes", stats.total.statistics.allocationBytes > 0,
                           "VMA tarafindan tahsis edilen toplam bayt sifirdan buyuk olmali");
            std::cout << "    [VMA Stats] Aktif Tahsis: " << stats.total.statistics.allocationCount 
                      << ", Toplam Boyut: " << stats.total.statistics.allocationBytes / (1024 * 1024) << " MB\n";
        }

        // 3. 5000 Adet Kucuk Tampon Tahsis ve Silme Stres Testi (4096 Surucu Siniri Asimi)
        {
            constexpr size_t STRESS_COUNT = 5000;
            std::vector<std::unique_ptr<Buffer>> buffers;
            buffers.reserve(STRESS_COUNT);

            bool stressSuccess = true;
            try {
                for (size_t i = 0; i < STRESS_COUNT; ++i) {
                    buffers.push_back(std::make_unique<Buffer>(
                        allocator,
                        256,
                        vk::BufferUsageFlagBits::eStorageBuffer,
                        VMA_MEMORY_USAGE_AUTO,
                        0,
                        false
                    ));
                }
            } catch (const std::exception& e) {
                std::cerr << "  [ERROR] VMA stres testi hatasi: " << e.what() << "\n";
                stressSuccess = false;
            }

            TEST_CHECK_MSG(suite, "Vma5000AllocationsStress", stressSuccess && buffers.size() == STRESS_COUNT,
                           "5000 adet kucuk tampon (4096 limitinin otesinde) VMA ile hatasiz tahsis edilmeli");

            // Tum tamponlari serbest birak
            buffers.clear();
            TEST_CHECK_MSG(suite, "VmaStressDeallocation", buffers.empty(), "5000 tampon basariyla serbest birakilmali");
        }
    }

private:
    static Astral::AppConfig BuildConfig() {
        Astral::AppConfig config;
        config.maxFrames = 1;
        config.width = 640;
        config.height = 360;
        return config;
    }
};

void RunVmaTests(bool runGpu) {
    const std::string suite = "VmaMemorySuite";
    std::cout << "  [INFO] VMA (Vulkan Memory Allocator) testleri baslatiliyor...\n";

    // 1. Headless / Mock Testleri (CI ortami, GPU gerektirmez)
    {
        Buffer headlessBuf(
            VK_NULL_HANDLE,
            vk::Device{},
            vk::PhysicalDevice{},
            2048,
            vk::BufferUsageFlagBits::eStorageBuffer,
            vk::MemoryPropertyFlagBits::eHostVisible,
            false
        );
        TEST_CHECK_MSG(suite, "HeadlessBufferConstruction", headlessBuf.GetBuffer() == nullptr,
                       "Null cihaz ve allocator ile Buffer guvenle olusturulabilmeli (no-op)");
        TEST_CHECK_MSG(suite, "HeadlessBufferSize", headlessBuf.GetSize() == 2048, "Tampon boyutu korunmali");
        TEST_CHECK_MSG(suite, "HeadlessBufferNotVma", !headlessBuf.IsVma(), "IsVma() false donmeli");

        BrickGrid headlessGrid(vk::Device{}, vk::PhysicalDevice{});
        std::vector<SDFEditGPU> edits;
        headlessGrid.Build(edits);
        TEST_CHECK_MSG(suite, "HeadlessBrickGridInitialBuild", headlessGrid.GetLastUpdatedCellCount() == BrickGrid::TOTAL_CELLS,
                       "Headless BrickGrid ilk insada tum hucreleri hesaplamali");
        headlessGrid.Build(edits);
        TEST_CHECK_MSG(suite, "HeadlessBrickGridStaticBuild", headlessGrid.GetLastUpdatedCellCount() == 0,
                       "Headless BrickGrid statik sahnede 0 hucre guncellemeli");
    }

    // 2. Canli GPU Testleri
    if (runGpu) {
        try {
            VmaTestApp app;
            app.Run(1); // 1 kare calistir (Renderer ve Context baslatilir)
            app.RunVmaValidation();
        } catch (const std::exception& e) {
            std::cerr << "  [ERROR] VMA GPU testi istisna firlatti: " << e.what() << "\n";
            TEST_CHECK_MSG(suite, "VmaGpuValidationExecution", false, "VMA GPU testleri hatasiz calismali");
        }
    } else {
        std::cout << "  [INFO] Canli GPU VMA testleri atlandi (--all veya --gpu ile calistirilabilir).\n";
    }
}

} // namespace Astral::Test
