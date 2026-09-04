#include "TestFramework.hpp"
#include "Astral/Renderer/BrickGrid.hpp"
#include "Astral/Renderer/SDFEdit.hpp"
#include <vector>
#include <iostream>

namespace Astral::Test {

void RunBrickGridTests() {
    const std::string suite = "BrickGridAccelerationSuite";

    // Headless test yapicisi (Vulkan Device olmaksizin CPU AABB ve Dirty Tracking dogrulamasi)
    BrickGrid grid;

    // 1. Ilk sahne: 2 primitif (Kure ve Kutu)
    std::vector<SDFEditGPU> edits(2);
    // Kure
    edits[0].position = glm::vec3(0.0f, 1.0f, 0.0f);
    edits[0].scale = glm::vec3(0.5f);
    edits[0].primitiveType = 0; // Sphere
    edits[0].operation = 0;     // Union
    edits[0].blendFactor = 0.25f;
    edits[0].albedo = glm::vec3(1.0f, 0.0f, 0.0f);
    edits[0].roughness = 0.5f;

    // Kutu
    edits[1].position = glm::vec3(3.0f, 1.0f, 0.0f);
    edits[1].scale = glm::vec3(0.5f);
    edits[1].primitiveType = 1; // Box
    edits[1].operation = 0;     // Union
    edits[1].blendFactor = 0.25f;
    edits[1].albedo = glm::vec3(0.0f, 1.0f, 0.0f);
    edits[1].roughness = 0.5f;

    // Test 1: Ilk insa — Tum hucreler hesaplanmalidir (Full Rebuild)
    grid.Build(edits);
    TEST_CHECK_MSG(suite, "InitialBuildUpdatesAllCells",
                   grid.GetLastUpdatedCellCount() == BrickGrid::TOTAL_CELLS,
                   "Ilk calismada grid'in tum hucreleri (16,384) hesaplanmalidir!");

    // Test 2: Statik Sahne — Hicbir primitif degismedi, Early-Out calismali (0 hucre)
    grid.Build(edits);
    TEST_CHECK_MSG(suite, "StaticSceneZeroUpdatedCells",
                   grid.GetLastUpdatedCellCount() == 0,
                   "Hicbir primitif degismediginde guncellenen hucre sayisi kesinlikle 0 olmalidir!");

    // Test 3: Tek Obje Hareketi — Kısmi AABB guncellemesi (Sayisal aralik kontrolu)
    edits[0].position.x += 0.5f;
    grid.Build(edits);
    size_t movedCellCount = grid.GetLastUpdatedCellCount();
    
    TEST_CHECK_MSG(suite, "SingleObjectMovedNonZero",
                   movedCellCount > 0,
                   "Tek obje hareket ettiginde guncellenen hucre sayisi 0'dan buyuk olmalidir!");
    TEST_CHECK_MSG(suite, "SingleObjectMovedLessThanTotal",
                   movedCellCount < BrickGrid::TOTAL_CELLS,
                   "Tek lokal obje hareketi tum grid'i yeniden insa etmemelidir!");
    TEST_CHECK_MSG(suite, "SingleObjectMovedExpectedRange",
                   movedCellCount >= 100 && movedCellCount <= 3000,
                   "Tek nesne hareketinde AABB marjini dahilinde beklenen 100-3000 hucre araligi saglanmalidir!");

    // Test 4: Hareketsiz sonraki kare tekrar 0 olmali
    grid.Build(edits);
    TEST_CHECK_MSG(suite, "PostMoveStaticZeroCells",
                   grid.GetLastUpdatedCellCount() == 0,
                   "Hareket sonrasi obje durdugunda tekrar 0 hucre guncellenmelidir!");

    // Test 5: Yalnizca Materyal Ozelligi Degisimi (SDF Mesafe Alani Sabit)
    // Albedo, roughness gibi ozellikler mesafeyi degistirmez; dirty tracking erken cikmalidir.
    edits[0].albedo = glm::vec3(0.2f, 0.8f, 0.4f);
    edits[0].roughness = 0.1f;
    edits[0].metallic = 0.9f;
    grid.Build(edits);
    TEST_CHECK_MSG(suite, "MaterialOnlyChangeZeroCells",
                   grid.GetLastUpdatedCellCount() == 0,
                   "Yalnizca materyal degisimlerinde grid mesafesi degismediginden 0 hucre guncellenmelidir!");

    // Test 6: Sonsuz Duzlem (Plane - primitiveType 3) Degisimi
    // Sonsuz etki alani nedeniyle duzlem hareketi Full Rebuild tetiklemelidir.
    SDFEditGPU planeEdit{};
    planeEdit.position = glm::vec3(0.0f, -1.0f, 0.0f);
    planeEdit.scale = glm::vec3(1.0f);
    planeEdit.primitiveType = 3; // Plane
    edits.push_back(planeEdit);

    // Primitif sayisi degistigi icin full rebuild tetiklenir
    grid.Build(edits);
    TEST_CHECK_MSG(suite, "PrimitiveCountChangeFullRebuild",
                   grid.GetLastUpdatedCellCount() == BrickGrid::TOTAL_CELLS,
                   "Primitif sayisi degisikligi tam yeniden insa tetiklemelidir!");

    // Duzlem yuksekligi degistirilir
    edits.back().position.y += 0.2f;
    grid.Build(edits);
    TEST_CHECK_MSG(suite, "InfinitePlaneChangeTriggersFullRebuild",
                   grid.GetLastUpdatedCellCount() == BrickGrid::TOTAL_CELLS,
                   "Sonsuz duzlem degisimi tum 16,384 hucrenin yeniden hesaplanmasini tetiklemelidir!");
}

} // namespace Astral::Test
