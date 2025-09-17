#include "Model.h"
#include "../../Core/Logger.h"
#include <filesystem>

namespace AstralEngine {

Model::Model(const std::string& filePath) 
    : m_filePath(filePath), m_isValid(false) {
    Logger::Debug("Model", "Creating model from file: '{}'", filePath);
}

void Model::AddMesh(std::shared_ptr<VulkanMesh> mesh) {
    if (mesh) {
        m_meshes.push_back(mesh);
        Logger::Debug("Model", "Added mesh to model '{}'. Total meshes: {}", 
                     m_filePath, m_meshes.size());
        m_isValid = true;
    } else {
        Logger::Warning("Model", "Attempted to add null mesh to model '{}'", m_filePath);
    }
}

const std::vector<std::shared_ptr<VulkanMesh>>& Model::GetMeshes() const {
    return m_meshes;
}

size_t Model::GetMeshCount() const {
    return m_meshes.size();
}

const std::string& Model::GetFilePath() const {
    return m_filePath;
}

std::string Model::GetName() const {
    // Dosya yolundan sadece dosya adını çıkar
    return std::filesystem::path(m_filePath).filename().string();
}

bool Model::IsValid() const {
    return m_isValid && !m_meshes.empty();
}

void Model::Invalidate() {
    m_isValid = false;
    m_meshes.clear();
    Logger::Warning("Model", "Model '{}' invalidated", m_filePath);
}

} // namespace AstralEngine
