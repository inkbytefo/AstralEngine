#include "AssetHandle.h"
#include <functional>
#include "../../Core/Logger.h"

namespace AstralEngine {

AssetHandle::AssetHandle() 
    : m_id(0), m_type(Type::Unknown), m_path("") {
}

AssetHandle::AssetHandle(uint64_t id, Type type) 
    : m_id(id), m_type(type), m_path("") {
}

AssetHandle::AssetHandle(const std::string& path, Type type) 
    : m_type(type), m_path(path) {
    m_id = GenerateID(path);
}

bool AssetHandle::operator==(const AssetHandle& other) const {
    return m_id == other.m_id && m_type == other.m_type;
}

bool AssetHandle::operator!=(const AssetHandle& other) const {
    return !(*this == other);
}

bool AssetHandle::operator<(const AssetHandle& other) const {
    if (m_type != other.m_type) {
        return static_cast<int>(m_type) < static_cast<int>(other.m_type);
    }
    return m_id < other.m_id;
}

bool AssetHandle::IsValid() const {
    return m_id != 0 && !m_path.empty();
}

uint64_t AssetHandle::GetID() const {
    return m_id;
}

AssetHandle::Type AssetHandle::GetType() const {
    return m_type;
}

const std::string& AssetHandle::GetPath() const {
    return m_path;
}

size_t AssetHandle::GetHash() const {
    // ID ve type'ı birleştirerek hash oluştur
    return static_cast<size_t>(m_id) ^ (static_cast<size_t>(m_type) << 32);
}

uint64_t AssetHandle::GenerateID(const std::string& path) {
    if (path.empty()) {
        return 0;
    }
    
    // std::hash kullanarak path'ten benzersiz bir ID üret
    std::hash<std::string> hasher;
    return static_cast<uint64_t>(hasher(path));
}

// AssetHandleHash implementasyonu
size_t AssetHandleHash::operator()(const AssetHandle& handle) const {
    return handle.GetHash();
}

} // namespace AstralEngine
