#pragma once

#include <cstdint>
#include <string>
#include <functional>

namespace AstralEngine {

/**
 * @brief Asset'leri benzersiz şekilde tanımlamak için kullanılan handle
 * 
 * String path yerine daha verimli ve type-safe bir asset referans sistemi sağlar.
 */
class AssetHandle {
public:
    // Asset tipleri
    enum class Type {
        Model,
        Texture,
        Shader,
        Audio,
        Material,
        Unknown
    };

    AssetHandle();
    explicit AssetHandle(uint64_t id, Type type = Type::Unknown);
    explicit AssetHandle(const std::string& path, Type type = Type::Unknown);
    
    // Comparison operators
    bool operator==(const AssetHandle& other) const;
    bool operator!=(const AssetHandle& other) const;
    bool operator<(const AssetHandle& other) const;
    
    // Utility functions
    bool IsValid() const;
    uint64_t GetID() const;
    Type GetType() const;
    const std::string& GetPath() const;
    
    // Hash function support
    size_t GetHash() const;

private:
    uint64_t m_id;
    Type m_type;
    std::string m_path;
    
    // Path'ten ID üretmek için
    static uint64_t GenerateID(const std::string& path);
};

// Hash function for std::unordered_map
struct AssetHandleHash {
    size_t operator()(const AssetHandle& handle) const;
};

} // namespace AstralEngine

// std::hash specialization for AssetHandle - sadece bir kez tanımlanmalı
#ifndef ASSET_HANDLE_HASH_DEFINED
#define ASSET_HANDLE_HASH_DEFINED
namespace std {
    template<>
    struct hash<AstralEngine::AssetHandle> {
        size_t operator()(const AstralEngine::AssetHandle& handle) const {
            return handle.GetHash();
        }
    };
}
#endif
