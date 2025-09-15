# Jolt Physics Entegrasyon Rehberi - Astral Engine

**Tarih:** 10 Eylül 2025  
**Jolt Physics Son Sürüm:** v5.3.0 (15 Mart 2025)  
**Engine Hedef:** Astral Engine v0.1.0-alpha

## 📋 İçindekiler

1. [Jolt Physics Hakkında Genel Bilgiler](#jolt-physics-hakkında-genel-bilgiler)
2. [Sistem Gereksinimleri](#sistem-gereksinimleri)
3. [Jolt Physics İndirme ve Kurulum](#jolt-physics-indirme-ve-kurulum)
4. [CMake Entegrasyonu](#cmake-entegrasyonu)
5. [Kod Entegrasyonu](#kod-entegrasyonu)
6. [PhysicsSubsystem İmplementasyonu](#physicssubsystem-implementasyonu)
7. [Rigid Body Yönetimi](#rigid-body-yönetimi)
8. [Collision Detection ve Shapes](#collision-detection-ve-shapes)
9. [Constraints ve Joints](#constraints-ve-joints)
10. [Soft Body Physics](#soft-body-physics)
11. [Character Controllers](#character-controllers)
12. [Vehicle Physics](#vehicle-physics)
13. [Performance Optimizasyonları](#performance-optimizasyonları)
14. [Debugging ve Profiling](#debugging-ve-profiling)
15. [Test ve Doğrulama](#test-ve-doğrulama)

---

## 🎯 Jolt Physics Hakkında Genel Bilgiler

### Jolt Physics Nedir?

Jolt Physics, **Horizon Forbidden West** ve **Death Stranding 2** gibi AAA oyunlarda kullanılan, multi-core friendly bir rigid body physics ve collision detection kütüphanesidir. Modern C++17 ile yazılmış olup, oyunlar ve VR uygulamaları için optimize edilmiştir.

### 🌟 Jolt Physics 5.3.0'ın Temel Özellikleri

**Rigid Body Physics:**
- Sphere, Box, Capsule, Cylinder, Convex Hull gibi çeşitli şekiller
- Compound shapes ve mesh shapes
- Terrain (height field) desteği
- Continuous collision detection (CCD)
- Multi-threaded simulation

**Advanced Features:**
- **Soft Body Physics** - Cloth, soft balls, deformable objects
- **Character Controllers** - Virtual ve rigid body character'lar
- **Vehicle Physics** - Wheeled vehicles, tracked vehicles, motorcycles
- **Constraint System** - Fixed, Hinge, Distance, 6DOF, vs.
- **Ragdoll Physics** - Animated ragdoll mapping

**Performance Features:**
- Multi-core friendly architecture
- Deterministic simulation
- SIMD optimizations (SSE, AVX, NEON)
- Memory-efficient design
- Up to 110M triangle mesh support

### 🔧 **Desteklenen Platformlar**
- **Windows**: Desktop/UWP x86/x64/ARM32/ARM64
- **Linux**: Ubuntu, x86/x64/ARM32/ARM64/RISC-V64/LoongArch64/PowerPC64LE  
- **macOS**: x64/ARM64 (Apple Silicon)
- **iOS**: x64/ARM64
- **Android**: x86/x64/ARM32/ARM64
- **WebAssembly**: WASM32/WASM64

---

## 🛠️ Sistem Gereksinimleri

### Minimum Gereksinimler

- **CMake:** 3.20+ (önerilen)
- **C++ Compiler:**
  - MSVC 2019+ (Windows)
  - Clang 10+ (macOS/Linux)
  - GCC 9+ (Linux)
- **CPU Requirements:**
  - x86/x64: SSE2 minimum (SSE4.1/4.2, AVX, AVX2, AVX512 desteklenir)
  - ARM64: NEON ve FP16
  - ARM32: özel instruction gerekmez

### Platform Özel Gereksinimler

**Windows:**
- Visual Studio 2019+ veya mingw-w64
- CMake 3.20+

**Linux:**
- GCC 9+ veya Clang 10+
- CMake 3.20+

**macOS:**
- Xcode 12+
- CMake 3.23+

---

## 📦 Jolt Physics İndirme ve Kurulum

### Method 1: Git Clone (Önerilen)

```bash
# Jolt Physics repository'sini klonla
git clone https://github.com/jrouwe/JoltPhysics.git

# External dizini altına taşı
mv JoltPhysics External/JoltPhysics

# veya submodule olarak ekle
git submodule add https://github.com/jrouwe/JoltPhysics.git External/JoltPhysics
git submodule update --init --recursive
```

### Method 2: CMake FetchContent

```cmake
include(FetchContent)

FetchContent_Declare(
    JoltPhysics
    GIT_REPOSITORY https://github.com/jrouwe/JoltPhysics.git
    GIT_TAG v5.3.0
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(JoltPhysics)
```

### Method 3: Release Archive

```bash
# En son release'i indir
wget https://github.com/jrouwe/JoltPhysics/archive/refs/tags/v5.3.0.zip
unzip v5.3.0.zip
mv JoltPhysics-5.3.0 External/JoltPhysics
```

---

## 🔧 CMake Entegrasyonu

### 1. Ana CMakeLists.txt Güncellemeleri

AstralEngine'deki `CMakeLists.txt` dosyasını güncelleyin:

```cmake
# Jolt Physics seçenekleri
option(USE_JOLT_PHYSICS "Enable Jolt Physics" ON)

if(USE_JOLT_PHYSICS)
    message(STATUS "Jolt Physics enabled")
    
    # Jolt Physics konfigürasyon
    set(TARGET_UNIT_TESTS OFF CACHE BOOL "Build Unit Tests" FORCE)
    set(TARGET_HELLO_WORLD OFF CACHE BOOL "Build Hello World" FORCE)
    set(TARGET_PERFORMANCE_TEST OFF CACHE BOOL "Build Performance Test" FORCE)
    set(TARGET_SAMPLES OFF CACHE BOOL "Build Samples" FORCE)
    set(TARGET_VIEWER OFF CACHE BOOL "Build Viewer" FORCE)
    
    # Debug/Release konfigürasyonu
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(ENABLE_ASSERTS ON CACHE BOOL "Enable Asserts" FORCE)
        set(DEBUG_RENDERER_IN_DEBUG_AND_RELEASE ON CACHE BOOL "Enable Debug Renderer" FORCE)
    else()
        set(ENABLE_ASSERTS OFF CACHE BOOL "Enable Asserts" FORCE)
        set(DEBUG_RENDERER_IN_DEBUG_AND_RELEASE OFF CACHE BOOL "Enable Debug Renderer" FORCE)
    endif()
    
    # Cross-platform determinism
    set(CROSS_PLATFORM_DETERMINISTIC ON CACHE BOOL "Cross Platform Deterministic" FORCE)
    set(FLOATING_POINT_EXCEPTIONS_ENABLED OFF CACHE BOOL "Floating Point Exceptions" FORCE)
    
    # Profiler desteği
    set(PROFILER_ENABLED ON CACHE BOOL "Profiler Enabled" FORCE)
    
    # Jolt Physics alt dizini ekle
    add_subdirectory(External/JoltPhysics/Build)
    
    # Astral Engine'e Jolt linkle
    target_link_libraries(AstralEngine PRIVATE Jolt)
    target_compile_definitions(AstralEngine PRIVATE ASTRAL_USE_JOLT_PHYSICS)
    
    # Include dizinini ekle
    target_include_directories(AstralEngine PRIVATE External/JoltPhysics)
    
    message(STATUS "Jolt Physics integrated successfully")
else()
    message(STATUS "Jolt Physics disabled")
endif()
```

### 2. Platform Özel Ayarlar

```cmake
# Platform özel konfigürasyonlar
if(USE_JOLT_PHYSICS)
    if(WIN32)
        # Windows özel ayarlar
        target_compile_definitions(AstralEngine PRIVATE JPH_SHARED_LIBRARY=0)
    elseif(APPLE)
        # macOS/iOS özel ayarlar
        if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
            set(IOS ON CACHE BOOL "iOS Build" FORCE)
        endif()
    elseif(ANDROID)
        # Android özel ayarlar
        set(USE_SSE4_1 OFF CACHE BOOL "Use SSE4.1" FORCE)
        set(USE_SSE4_2 OFF CACHE BOOL "Use SSE4.2" FORCE)
        set(USE_AVX OFF CACHE BOOL "Use AVX" FORCE)
        set(USE_AVX2 OFF CACHE BOOL "Use AVX2" FORCE)
    elseif(UNIX)
        # Linux özel ayarlar
        target_link_libraries(AstralEngine PRIVATE pthread)
    endif()
    
    # SIMD optimizasyonları (x86/x64 için)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|x86")
        target_compile_definitions(AstralEngine PRIVATE 
            JPH_USE_SSE4_1=1
            JPH_USE_SSE4_2=1
            JPH_USE_AVX=1
            JPH_USE_AVX2=1
        )
    endif()
endif()

# Konfigürasyon özeti
if(USE_JOLT_PHYSICS)
    message(STATUS "=== Jolt Physics Configuration ===")
    message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
    message(STATUS "Asserts: ${ENABLE_ASSERTS}")
    message(STATUS "Debug Renderer: ${DEBUG_RENDERER_IN_DEBUG_AND_RELEASE}")
    message(STATUS "Cross Platform Deterministic: ${CROSS_PLATFORM_DETERMINISTIC}")
    message(STATUS "Profiler: ${PROFILER_ENABLED}")
    message(STATUS "===================================")
endif()
```

---

## 💻 Kod Entegrasyonu

### 1. PhysicsTypes.h - Temel Tipler

`Source/Physics/PhysicsTypes.h` dosyasını oluşturun:

```cpp
#pragma once

#ifdef ASTRAL_USE_JOLT_PHYSICS
    // Jolt Physics headers - her zaman Jolt.h'ı ilk include et
    #include <Jolt/Jolt.h>
    #include <Jolt/RegisterTypes.h>
    #include <Jolt/Core/Factory.h>
    #include <Jolt/Core/TempAllocator.h>
    #include <Jolt/Core/JobSystemThreadPool.h>
    #include <Jolt/Physics/PhysicsSettings.h>
    #include <Jolt/Physics/PhysicsSystem.h>
    #include <Jolt/Physics/Collision/Shape/BoxShape.h>
    #include <Jolt/Physics/Collision/Shape/SphereShape.h>
    #include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
    #include <Jolt/Physics/Body/BodyCreationSettings.h>
    #include <Jolt/Physics/Body/BodyActivationListener.h>
#endif

#include "../../Core/Math/Vector3.h"
#include "../../Core/Math/Quaternion.h"

namespace AstralEngine {

#ifdef ASTRAL_USE_JOLT_PHYSICS
    // Jolt Physics tür aliasları
    using PhysicsFloat = JPH::Float;
    using PhysicsVec3 = JPH::Vec3;
    using PhysicsQuat = JPH::Quat;
    using PhysicsBodyID = JPH::BodyID;
    using PhysicsShapeRef = JPH::RefConst<JPH::Shape>;
    
    // Motion types
    enum class MotionType : uint8_t {
        Static = static_cast<uint8_t>(JPH::EMotionType::Static),
        Kinematic = static_cast<uint8_t>(JPH::EMotionType::Kinematic),
        Dynamic = static_cast<uint8_t>(JPH::EMotionType::Dynamic)
    };
    
    // Shape types
    enum class PhysicsShapeType : uint8_t {
        Sphere,
        Box,
        Capsule,
        Cylinder,
        ConvexHull,
        Mesh,
        HeightField,
        Compound
    };
    
#else
    // Physics desteği yoksa dummy types
    using PhysicsFloat = float;
    using PhysicsVec3 = Vector3;
    using PhysicsQuat = Quaternion;
    using PhysicsBodyID = uint32_t;
    using PhysicsShapeRef = void*;
    
    enum class MotionType : uint8_t {
        Static, Kinematic, Dynamic
    };
    
    enum class PhysicsShapeType : uint8_t {
        Sphere, Box, Capsule, Cylinder, ConvexHull, Mesh, HeightField, Compound
    };
#endif

// Conversion utilities
#ifdef ASTRAL_USE_JOLT_PHYSICS
    inline PhysicsVec3 ToJolt(const Vector3& v) {
        return PhysicsVec3(v.x, v.y, v.z);
    }
    
    inline Vector3 FromJolt(const PhysicsVec3& v) {
        return Vector3(v.GetX(), v.GetY(), v.GetZ());
    }
    
    inline PhysicsQuat ToJolt(const Quaternion& q) {
        return PhysicsQuat(q.x, q.y, q.z, q.w);
    }
    
    inline Quaternion FromJolt(const PhysicsQuat& q) {
        return Quaternion(q.GetX(), q.GetY(), q.GetZ(), q.GetW());
    }
#else
    inline PhysicsVec3 ToJolt(const Vector3& v) { return v; }
    inline Vector3 FromJolt(const PhysicsVec3& v) { return v; }
    inline PhysicsQuat ToJolt(const Quaternion& q) { return q; }
    inline Quaternion FromJolt(const PhysicsQuat& q) { return q; }
#endif

// Physics layer definitions
namespace PhysicsLayers {
    static constexpr uint16_t NON_MOVING = 0;
    static constexpr uint16_t MOVING = 1;
    static constexpr uint16_t DEBRIS = 2; // Small objects
    static constexpr uint16_t SENSOR = 3;
    static constexpr uint16_t CHARACTER = 4;
    static constexpr uint16_t VEHICLE = 5;
    static constexpr uint16_t NUM_LAYERS = 6;
}

// Broadphase layers
namespace BroadPhaseLayers {
    static constexpr uint8_t NON_MOVING = 0;
    static constexpr uint8_t MOVING = 1;
    static constexpr uint8_t DEBRIS = 2;
    static constexpr uint8_t NUM_LAYERS = 3;
}

} // namespace AstralEngine
```

### 2. PhysicsSystem Wrapper

`Source/Physics/PhysicsSystem.h` dosyasını oluşturun:

```cpp
#pragma once

#include "PhysicsTypes.h"
#include "../../Core/Memory/Memory.h"
#include "../../Core/Logger.h"

#ifdef ASTRAL_USE_JOLT_PHYSICS
    #include <Jolt/Physics/Collision/ObjectLayer.h>
    #include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#endif

namespace AstralEngine {

#ifdef ASTRAL_USE_JOLT_PHYSICS

// Broadphase layer interface implementation
class BroadPhaseLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface {
public:
    BroadPhaseLayerInterfaceImpl();

    uint32_t GetNumBroadPhaseLayers() const override;
    JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override;

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override;
#endif

private:
    JPH::BroadPhaseLayer m_objectToBroadPhase[PhysicsLayers::NUM_LAYERS];
};

// Object vs broadphase layer filter
class ObjectVsBroadPhaseLayerFilterImpl : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer layer1, JPH::BroadPhaseLayer layer2) const override;
};

// Object vs object layer filter
class ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer object1, JPH::ObjectLayer object2) const override;
};

// Contact listener
class ContactListenerImpl : public JPH::ContactListener {
public:
    JPH::ValidateResult OnContactValidate(
        const JPH::Body& body1, const JPH::Body& body2,
        JPH::RVec3Arg baseOffset, const JPH::CollideShapeResult& collisionResult
    ) override;

    void OnContactAdded(
        const JPH::Body& body1, const JPH::Body& body2,
        const JPH::ContactManifold& manifold, JPH::ContactSettings& settings
    ) override;

    void OnContactPersisted(
        const JPH::Body& body1, const JPH::Body& body2,
        const JPH::ContactManifold& manifold, JPH::ContactSettings& settings
    ) override;

    void OnContactRemoved(const JPH::SubShapeIDPair& subShapePair) override;
};

// Body activation listener
class BodyActivationListenerImpl : public JPH::BodyActivationListener {
public:
    void OnBodyActivated(const JPH::BodyID& bodyID, uint64_t bodyUserData) override;
    void OnBodyDeactivated(const JPH::BodyID& bodyID, uint64_t bodyUserData) override;
};

#endif // ASTRAL_USE_JOLT_PHYSICS

/**
 * @brief Ana physics system wrapper sınıfı
 */
class PhysicsSystem {
public:
    PhysicsSystem();
    ~PhysicsSystem();

    // Non-copyable
    PhysicsSystem(const PhysicsSystem&) = delete;
    PhysicsSystem& operator=(const PhysicsSystem&) = delete;

    // Yaşam döngüsü
    bool Initialize(uint32_t maxBodies = 65536, 
                   uint32_t numBodyMutexes = 0, 
                   uint32_t maxBodyPairs = 65536, 
                   uint32_t maxContactConstraints = 10240);
    void Update(float deltaTime, int collisionSteps = 1);
    void Shutdown();

    // Body management
    PhysicsBodyID CreateBody(const Vector3& position, 
                           const Quaternion& rotation, 
                           MotionType motionType = MotionType::Dynamic,
                           PhysicsShapeType shapeType = PhysicsShapeType::Box,
                           const Vector3& halfExtents = Vector3(0.5f));
    
    void DestroyBody(PhysicsBodyID bodyID);
    void SetBodyPosition(PhysicsBodyID bodyID, const Vector3& position);
    void SetBodyRotation(PhysicsBodyID bodyID, const Quaternion& rotation);
    void SetBodyVelocity(PhysicsBodyID bodyID, const Vector3& velocity);
    void SetBodyAngularVelocity(PhysicsBodyID bodyID, const Vector3& angularVelocity);
    
    Vector3 GetBodyPosition(PhysicsBodyID bodyID) const;
    Quaternion GetBodyRotation(PhysicsBodyID bodyID) const;
    Vector3 GetBodyVelocity(PhysicsBodyID bodyID) const;
    Vector3 GetBodyAngularVelocity(PhysicsBodyID bodyID) const;
    
    void ActivateBody(PhysicsBodyID bodyID);
    void DeactivateBody(PhysicsBodyID bodyID);
    bool IsBodyActive(PhysicsBodyID bodyID) const;

    // Collision queries
    struct RaycastResult {
        bool hit = false;
        Vector3 hitPoint;
        Vector3 normal;
        float fraction = 0.0f;
        PhysicsBodyID bodyID;
    };

    RaycastResult Raycast(const Vector3& origin, const Vector3& direction, float maxDistance = 1000.0f);
    
    // Debug rendering
    void DrawDebugGeometry();

    // Getters
#ifdef ASTRAL_USE_JOLT_PHYSICS
    JPH::PhysicsSystem* GetJoltSystem() const { return m_physicsSystem.get(); }
    JPH::BodyInterface& GetBodyInterface() const;
#endif

private:
    bool m_initialized = false;

#ifdef ASTRAL_USE_JOLT_PHYSICS
    // Jolt specific members
    std::unique_ptr<JPH::Factory> m_factory;
    std::unique_ptr<JPH::TempAllocatorImpl> m_tempAllocator;
    std::unique_ptr<JPH::JobSystemThreadPool> m_jobSystem;
    std::unique_ptr<BroadPhaseLayerInterfaceImpl> m_broadPhaseLayerInterface;
    std::unique_ptr<ObjectVsBroadPhaseLayerFilterImpl> m_objectVsBroadPhaseLayerFilter;
    std::unique_ptr<ObjectLayerPairFilterImpl> m_objectLayerPairFilter;
    std::unique_ptr<JPH::PhysicsSystem> m_physicsSystem;
    std::unique_ptr<ContactListenerImpl> m_contactListener;
    std::unique_ptr<BodyActivationListenerImpl> m_bodyActivationListener;
#endif

    void InitializeJolt();
    void ShutdownJolt();
};

} // namespace AstralEngine
```

### 3. PhysicsSystem Implementation

`Source/Physics/PhysicsSystem.cpp` dosyasını oluşturun:

```cpp
#include "PhysicsSystem.h"
#include "../../Core/Logger.h"

#ifdef ASTRAL_USE_JOLT_PHYSICS
    #include <Jolt/Physics/Collision/Shape/BoxShape.h>
    #include <Jolt/Physics/Collision/Shape/SphereShape.h>
    #include <Jolt/Physics/Collision/RayCast.h>
    #include <Jolt/Physics/Collision/CastResult.h>
    #include <thread>
#endif

namespace AstralEngine {

#ifdef ASTRAL_USE_JOLT_PHYSICS

// BroadPhase Layer Interface Implementation
BroadPhaseLayerInterfaceImpl::BroadPhaseLayerInterfaceImpl() {
    // Map object layers to broadphase layers
    m_objectToBroadPhase[PhysicsLayers::NON_MOVING] = JPH::BroadPhaseLayer(BroadPhaseLayers::NON_MOVING);
    m_objectToBroadPhase[PhysicsLayers::MOVING] = JPH::BroadPhaseLayer(BroadPhaseLayers::MOVING);
    m_objectToBroadPhase[PhysicsLayers::DEBRIS] = JPH::BroadPhaseLayer(BroadPhaseLayers::DEBRIS);
    m_objectToBroadPhase[PhysicsLayers::SENSOR] = JPH::BroadPhaseLayer(BroadPhaseLayers::MOVING);
    m_objectToBroadPhase[PhysicsLayers::CHARACTER] = JPH::BroadPhaseLayer(BroadPhaseLayers::MOVING);
    m_objectToBroadPhase[PhysicsLayers::VEHICLE] = JPH::BroadPhaseLayer(BroadPhaseLayers::MOVING);
}

uint32_t BroadPhaseLayerInterfaceImpl::GetNumBroadPhaseLayers() const {
    return BroadPhaseLayers::NUM_LAYERS;
}

JPH::BroadPhaseLayer BroadPhaseLayerInterfaceImpl::GetBroadPhaseLayer(JPH::ObjectLayer layer) const {
    JPH_ASSERT(layer < PhysicsLayers::NUM_LAYERS);
    return m_objectToBroadPhase[layer];
}

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
const char* BroadPhaseLayerInterfaceImpl::GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const {
    switch ((JPH::BroadPhaseLayer::Type)layer) {
        case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::NON_MOVING: return "NON_MOVING";
        case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::MOVING: return "MOVING";
        case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::DEBRIS: return "DEBRIS";
        default: JPH_ASSERT(false); return "INVALID";
    }
}
#endif

// Object vs BroadPhase Layer Filter
bool ObjectVsBroadPhaseLayerFilterImpl::ShouldCollide(JPH::ObjectLayer layer1, JPH::BroadPhaseLayer layer2) const {
    switch (layer1) {
        case PhysicsLayers::NON_MOVING:
            return layer2 == JPH::BroadPhaseLayer(BroadPhaseLayers::MOVING) || 
                   layer2 == JPH::BroadPhaseLayer(BroadPhaseLayers::DEBRIS);
        case PhysicsLayers::MOVING:
        case PhysicsLayers::DEBRIS:
        case PhysicsLayers::SENSOR:
        case PhysicsLayers::CHARACTER:
        case PhysicsLayers::VEHICLE:
            return true;
        default:
            JPH_ASSERT(false);
            return false;
    }
}

// Object Layer Pair Filter
bool ObjectLayerPairFilterImpl::ShouldCollide(JPH::ObjectLayer object1, JPH::ObjectLayer object2) const {
    switch (object1) {
        case PhysicsLayers::NON_MOVING:
            return object2 == PhysicsLayers::MOVING || 
                   object2 == PhysicsLayers::DEBRIS ||
                   object2 == PhysicsLayers::CHARACTER ||
                   object2 == PhysicsLayers::VEHICLE;
        case PhysicsLayers::MOVING:
        case PhysicsLayers::DEBRIS:
            return true;
        case PhysicsLayers::SENSOR:
            return object2 == PhysicsLayers::MOVING || 
                   object2 == PhysicsLayers::DEBRIS ||
                   object2 == PhysicsLayers::CHARACTER ||
                   object2 == PhysicsLayers::VEHICLE;
        case PhysicsLayers::CHARACTER:
        case PhysicsLayers::VEHICLE:
            return object2 != PhysicsLayers::SENSOR; // Characters don't trigger sensors
        default:
            JPH_ASSERT(false);
            return false;
    }
}

// Contact Listener Implementation
JPH::ValidateResult ContactListenerImpl::OnContactValidate(
    const JPH::Body& body1, const JPH::Body& body2, 
    JPH::RVec3Arg baseOffset, const JPH::CollideShapeResult& collisionResult
) {
    Logger::Debug("Jolt", "Contact validate between body {} and {}", 
                  body1.GetID().GetIndex(), body2.GetID().GetIndex());
    return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
}

void ContactListenerImpl::OnContactAdded(
    const JPH::Body& body1, const JPH::Body& body2, 
    const JPH::ContactManifold& manifold, JPH::ContactSettings& settings
) {
    Logger::Debug("Jolt", "Contact added between body {} and {}", 
                  body1.GetID().GetIndex(), body2.GetID().GetIndex());
    
    // TODO: Astral Engine event system ile collision event gönder
}

void ContactListenerImpl::OnContactPersisted(
    const JPH::Body& body1, const JPH::Body& body2, 
    const JPH::ContactManifold& manifold, JPH::ContactSettings& settings
) {
    // Persistent contact handling
}

void ContactListenerImpl::OnContactRemoved(const JPH::SubShapeIDPair& subShapePair) {
    Logger::Debug("Jolt", "Contact removed");
}

// Body Activation Listener Implementation
void BodyActivationListenerImpl::OnBodyActivated(const JPH::BodyID& bodyID, uint64_t bodyUserData) {
    Logger::Debug("Jolt", "Body {} activated", bodyID.GetIndex());
}

void BodyActivationListenerImpl::OnBodyDeactivated(const JPH::BodyID& bodyID, uint64_t bodyUserData) {
    Logger::Debug("Jolt", "Body {} deactivated", bodyID.GetIndex());
}

#endif // ASTRAL_USE_JOLT_PHYSICS

// PhysicsSystem Implementation
PhysicsSystem::PhysicsSystem() {
    Logger::Debug("PhysicsSystem", "PhysicsSystem created");
}

PhysicsSystem::~PhysicsSystem() {
    if (m_initialized) {
        Shutdown();
    }
    Logger::Debug("PhysicsSystem", "PhysicsSystem destroyed");
}

bool PhysicsSystem::Initialize(uint32_t maxBodies, uint32_t numBodyMutexes, 
                              uint32_t maxBodyPairs, uint32_t maxContactConstraints) {
    if (m_initialized) {
        Logger::Warning("PhysicsSystem", "PhysicsSystem already initialized");
        return true;
    }

    Logger::Info("PhysicsSystem", "Initializing PhysicsSystem...");
    Logger::Info("PhysicsSystem", "  Max Bodies: {}", maxBodies);
    Logger::Info("PhysicsSystem", "  Max Body Pairs: {}", maxBodyPairs);
    Logger::Info("PhysicsSystem", "  Max Contact Constraints: {}", maxContactConstraints);

#ifdef ASTRAL_USE_JOLT_PHYSICS
    InitializeJolt();

    // Create temp allocator
    m_tempAllocator = std::make_unique<JPH::TempAllocatorImpl>(10 * 1024 * 1024); // 10 MB

    // Create job system
    uint32_t maxConcurrency = std::max(1u, std::thread::hardware_concurrency() - 1);
    m_jobSystem = std::make_unique<JPH::JobSystemThreadPool>(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, maxConcurrency);

    // Layer interfaces
    m_broadPhaseLayerInterface = std::make_unique<BroadPhaseLayerInterfaceImpl>();
    m_objectVsBroadPhaseLayerFilter = std::make_unique<ObjectVsBroadPhaseLayerFilterImpl>();
    m_objectLayerPairFilter = std::make_unique<ObjectLayerPairFilterImpl>();

    // Physics system
    m_physicsSystem = std::make_unique<JPH::PhysicsSystem>();
    m_physicsSystem->Init(maxBodies, 
                         numBodyMutexes > 0 ? numBodyMutexes : 0, 
                         maxBodyPairs, 
                         maxContactConstraints,
                         *m_broadPhaseLayerInterface,
                         *m_objectVsBroadPhaseLayerFilter,
                         *m_objectLayerPairFilter);

    // Contact listener
    m_contactListener = std::make_unique<ContactListenerImpl>();
    m_physicsSystem->SetContactListener(m_contactListener.get());

    // Body activation listener
    m_bodyActivationListener = std::make_unique<BodyActivationListenerImpl>();
    m_physicsSystem->SetBodyActivationListener(m_bodyActivationListener.get());

    Logger::Info("PhysicsSystem", "Jolt Physics initialized successfully");
    Logger::Info("PhysicsSystem", "  Version: {}.{}.{}", 
                 JPH_VERSION_MAJOR, JPH_VERSION_MINOR, JPH_VERSION_PATCH);
    Logger::Info("PhysicsSystem", "  Hardware Concurrency: {}", maxConcurrency);

#else
    Logger::Warning("PhysicsSystem", "Jolt Physics not available - using null implementation");
#endif

    m_initialized = true;
    return true;
}

void PhysicsSystem::Update(float deltaTime, int collisionSteps) {
    if (!m_initialized) {
        return;
    }

#ifdef ASTRAL_USE_JOLT_PHYSICS
    // Jolt physics update
    if (m_physicsSystem) {
        // Physics settings için varsayılan değerler
        const int cCollisionSteps = collisionSteps;
        
        JPH::PhysicsSettings settings;
        m_physicsSystem->Update(deltaTime, cCollisionSteps, settings, m_jobSystem.get(), m_tempAllocator.get());
    }
#endif
}

void PhysicsSystem::Shutdown() {
    if (!m_initialized) {
        return;
    }

    Logger::Info("PhysicsSystem", "Shutting down PhysicsSystem...");

#ifdef ASTRAL_USE_JOLT_PHYSICS
    ShutdownJolt();
#endif

    m_initialized = false;
    Logger::Info("PhysicsSystem", "PhysicsSystem shutdown complete");
}

PhysicsBodyID PhysicsSystem::CreateBody(const Vector3& position, const Quaternion& rotation, 
                                      MotionType motionType, PhysicsShapeType shapeType, 
                                      const Vector3& halfExtents) {
#ifdef ASTRAL_USE_JOLT_PHYSICS
    if (!m_physicsSystem) {
        Logger::Error("PhysicsSystem", "Cannot create body: physics system not initialized");
        return JPH::BodyID();
    }

    // Shape oluştur
    JPH::RefConst<JPH::Shape> shape;
    switch (shapeType) {
        case PhysicsShapeType::Box:
            shape = new JPH::BoxShape(ToJolt(halfExtents));
            break;
        case PhysicsShapeType::Sphere:
            shape = new JPH::SphereShape(halfExtents.x);
            break;
        default:
            Logger::Error("PhysicsSystem", "Unsupported shape type");
            return JPH::BodyID();
    }

    // Body creation settings
    JPH::BodyCreationSettings bodySettings(shape, 
                                          ToJolt(position), 
                                          ToJolt(rotation), 
                                          static_cast<JPH::EMotionType>(motionType),
                                          motionType == MotionType::Static ? PhysicsLayers::NON_MOVING : PhysicsLayers::MOVING);

    // Body oluştur
    JPH::Body* body = m_physicsSystem->GetBodyInterface().CreateBody(bodySettings);
    if (!body) {
        Logger::Error("PhysicsSystem", "Failed to create body");
        return JPH::BodyID();
    }

    JPH::BodyID bodyID = body->GetID();
    m_physicsSystem->GetBodyInterface().AddBody(bodyID, JPH::EActivation::Activate);

    Logger::Debug("PhysicsSystem", "Created body with ID: {}", bodyID.GetIndex());
    return bodyID;
#else
    return static_cast<PhysicsBodyID>(0);
#endif
}

void PhysicsSystem::DestroyBody(PhysicsBodyID bodyID) {
#ifdef ASTRAL_USE_JOLT_PHYSICS
    if (!m_physicsSystem) {
        return;
    }

    m_physicsSystem->GetBodyInterface().RemoveBody(bodyID);
    m_physicsSystem->GetBodyInterface().DestroyBody(bodyID);
    Logger::Debug("PhysicsSystem", "Destroyed body with ID: {}", bodyID.GetIndex());
#endif
}

Vector3 PhysicsSystem::GetBodyPosition(PhysicsBodyID bodyID) const {
#ifdef ASTRAL_USE_JOLT_PHYSICS
    if (m_physicsSystem) {
        JPH::RVec3 position = m_physicsSystem->GetBodyInterface().GetCenterOfMassPosition(bodyID);
        return FromJolt(position);
    }
#endif
    return Vector3::Zero;
}

void PhysicsSystem::SetBodyPosition(PhysicsBodyID bodyID, const Vector3& position) {
#ifdef ASTRAL_USE_JOLT_PHYSICS
    if (m_physicsSystem) {
        m_physicsSystem->GetBodyInterface().SetPosition(bodyID, ToJolt(position), JPH::EActivation::Activate);
    }
#endif
}

PhysicsSystem::RaycastResult PhysicsSystem::Raycast(const Vector3& origin, const Vector3& direction, float maxDistance) {
    RaycastResult result;

#ifdef ASTRAL_USE_JOLT_PHYSICS
    if (!m_physicsSystem) {
        return result;
    }

    JPH::RayCast ray;
    ray.mOrigin = ToJolt(origin);
    ray.mDirection = ToJolt(direction.Normalized() * maxDistance);

    JPH::RayCastResult hitResult;
    if (m_physicsSystem->GetNarrowPhaseQuery().CastRay(ray, hitResult)) {
        result.hit = true;
        result.fraction = hitResult.mFraction;
        result.hitPoint = FromJolt(ray.mOrigin + ray.mDirection * hitResult.mFraction);
        result.normal = FromJolt(hitResult.mSurfaceNormal);
        result.bodyID = hitResult.mBodyID;
    }
#endif

    return result;
}

#ifdef ASTRAL_USE_JOLT_PHYSICS

void PhysicsSystem::InitializeJolt() {
    Logger::Info("PhysicsSystem", "Initializing Jolt Physics...");

    // Register allocation hook
    JPH::RegisterDefaultAllocator();

    // Install callbacks
    JPH::Trace = [](const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        char buffer[1024];
        vsnprintf(buffer, sizeof(buffer), fmt, args);
        va_end(args);
        Logger::Info("Jolt", "{}", buffer);
    };

    JPH_IF_ENABLE_ASSERTS(
        JPH::AssertFailed = [](const char* expression, const char* message, const char* file, JPH::uint line) {
            Logger::Error("Jolt", "Assert failed: {} in {}:{} - {}", expression, file, line, message);
            return true; // Trigger breakpoint
        };
    );

    // Create factory
    m_factory = std::make_unique<JPH::Factory>();
    JPH::Factory::sInstance = m_factory.get();

    // Register all physics types
    JPH::RegisterTypes();

    Logger::Info("PhysicsSystem", "Jolt Physics core initialized");
}

void PhysicsSystem::ShutdownJolt() {
    Logger::Info("PhysicsSystem", "Shutting down Jolt Physics...");

    // Clear physics system first
    if (m_physicsSystem) {
        m_physicsSystem.reset();
    }

    // Clear other components
    m_bodyActivationListener.reset();
    m_contactListener.reset();
    m_objectLayerPairFilter.reset();
    m_objectVsBroadPhaseLayerFilter.reset();
    m_broadPhaseLayerInterface.reset();
    m_jobSystem.reset();
    m_tempAllocator.reset();

    // Destroy factory and unregister types
    JPH::UnregisterTypes();

    if (m_factory) {
        JPH::Factory::sInstance = nullptr;
        m_factory.reset();
    }

    Logger::Info("PhysicsSystem", "Jolt Physics shutdown complete");
}

JPH::BodyInterface& PhysicsSystem::GetBodyInterface() const {
    return m_physicsSystem->GetBodyInterface();
}

#endif // ASTRAL_USE_JOLT_PHYSICS

} // namespace AstralEngine
```

---

## 🏗️ PhysicsSubsystem İmplementasyonu

### 1. PhysicsSubsystem Header

`Source/Subsystems/Physics/PhysicsSubsystem.h` dosyasını oluşturun:

```cpp
#pragma once

#include "../../Core/ISubsystem.h"
#include "../../Physics/PhysicsSystem.h"
#include <memory>

namespace AstralEngine {

/**
 * @brief Physics alt sistemi
 * 
 * Jolt Physics'i engine'e entegre eder ve physics simulation'ı yönetir.
 */
class PhysicsSubsystem : public ISubsystem {
public:
    PhysicsSubsystem();
    ~PhysicsSubsystem() override;

    // ISubsystem interface
    void OnInitialize(Engine* owner) override;
    void OnUpdate(float deltaTime) override;
    void OnShutdown() override;
    const char* GetName() const override { return "PhysicsSubsystem"; }

    // Physics interface
    PhysicsSystem* GetPhysicsSystem() const { return m_physicsSystem.get(); }

    // Convenience methods
    PhysicsBodyID CreateBox(const Vector3& position, const Vector3& halfExtents, MotionType motionType = MotionType::Dynamic);
    PhysicsBodyID CreateSphere(const Vector3& position, float radius, MotionType motionType = MotionType::Dynamic);
    PhysicsBodyID CreateCapsule(const Vector3& position, float radius, float height, MotionType motionType = MotionType::Dynamic);

    void DestroyBody(PhysicsBodyID bodyID);
    
    // Raycast utilities
    PhysicsSystem::RaycastResult Raycast(const Vector3& origin, const Vector3& direction, float maxDistance = 1000.0f);

    // Debug rendering
    void EnableDebugDrawing(bool enable) { m_debugDrawing = enable; }
    bool IsDebugDrawingEnabled() const { return m_debugDrawing; }

private:
    std::unique_ptr<PhysicsSystem> m_physicsSystem;
    Engine* m_owner = nullptr;
    bool m_debugDrawing = false;

    // Physics settings
    struct PhysicsSettings {
        uint32_t maxBodies = 65536;
        uint32_t maxBodyPairs = 65536;
        uint32_t maxContactConstraints = 10240;
        int collisionSteps = 1;
        float fixedTimeStep = 1.0f / 60.0f;
    } m_settings;

    float m_accumulator = 0.0f;
    void StepPhysics(float deltaTime);
};

} // namespace AstralEngine
```

### 2. PhysicsSubsystem Implementation

`Source/Subsystems/Physics/PhysicsSubsystem.cpp` dosyasını oluşturun:

```cpp
#include "PhysicsSubsystem.h"
#include "../../Core/Engine.h"
#include "../../Core/Logger.h"

namespace AstralEngine {

PhysicsSubsystem::PhysicsSubsystem() {
    Logger::Debug("PhysicsSubsystem", "PhysicsSubsystem created");
}

PhysicsSubsystem::~PhysicsSubsystem() {
    Logger::Debug("PhysicsSubsystem", "PhysicsSubsystem destroyed");
}

void PhysicsSubsystem::OnInitialize(Engine* owner) {
    m_owner = owner;
    Logger::Info("PhysicsSubsystem", "Initializing PhysicsSubsystem...");

    // Physics system oluştur
    m_physicsSystem = std::make_unique<PhysicsSystem>();
    
    if (!m_physicsSystem->Initialize(m_settings.maxBodies, 0, 
                                   m_settings.maxBodyPairs, 
                                   m_settings.maxContactConstraints)) {
        Logger::Error("PhysicsSubsystem", "Failed to initialize PhysicsSystem!");
        return;
    }

    Logger::Info("PhysicsSubsystem", "PhysicsSubsystem initialized successfully");

    // Test bodies oluştur (örnek)
    CreateTestBodies();
}

void PhysicsSubsystem::OnUpdate(float deltaTime) {
    if (!m_physicsSystem) {
        return;
    }

    // Fixed timestep physics simulation
    StepPhysics(deltaTime);

    // Debug drawing
    if (m_debugDrawing) {
        m_physicsSystem->DrawDebugGeometry();
    }
}

void PhysicsSubsystem::OnShutdown() {
    Logger::Info("PhysicsSubsystem", "Shutting down PhysicsSubsystem...");
    
    if (m_physicsSystem) {
        m_physicsSystem->Shutdown();
        m_physicsSystem.reset();
    }
    
    Logger::Info("PhysicsSubsystem", "PhysicsSubsystem shutdown complete");
}

void PhysicsSubsystem::StepPhysics(float deltaTime) {
    // Fixed timestep accumulation
    m_accumulator += deltaTime;
    
    while (m_accumulator >= m_settings.fixedTimeStep) {
        m_physicsSystem->Update(m_settings.fixedTimeStep, m_settings.collisionSteps);
        m_accumulator -= m_settings.fixedTimeStep;
    }
}

PhysicsBodyID PhysicsSubsystem::CreateBox(const Vector3& position, const Vector3& halfExtents, MotionType motionType) {
    if (!m_physicsSystem) {
        Logger::Error("PhysicsSubsystem", "Cannot create box: PhysicsSystem not initialized");
        return PhysicsBodyID();
    }

    return m_physicsSystem->CreateBody(position, Quaternion::Identity, motionType, 
                                     PhysicsShapeType::Box, halfExtents);
}

PhysicsBodyID PhysicsSubsystem::CreateSphere(const Vector3& position, float radius, MotionType motionType) {
    if (!m_physicsSystem) {
        Logger::Error("PhysicsSubsystem", "Cannot create sphere: PhysicsSystem not initialized");
        return PhysicsBodyID();
    }

    return m_physicsSystem->CreateBody(position, Quaternion::Identity, motionType,
                                     PhysicsShapeType::Sphere, Vector3(radius));
}

PhysicsBodyID PhysicsSubsystem::CreateCapsule(const Vector3& position, float radius, float height, MotionType motionType) {
    if (!m_physicsSystem) {
        Logger::Error("PhysicsSubsystem", "Cannot create capsule: PhysicsSystem not initialized");
        return PhysicsBodyID();
    }

    return m_physicsSystem->CreateBody(position, Quaternion::Identity, motionType,
                                     PhysicsShapeType::Capsule, Vector3(radius, height * 0.5f, radius));
}

void PhysicsSubsystem::DestroyBody(PhysicsBodyID bodyID) {
    if (m_physicsSystem) {
        m_physicsSystem->DestroyBody(bodyID);
    }
}

PhysicsSystem::RaycastResult PhysicsSubsystem::Raycast(const Vector3& origin, const Vector3& direction, float maxDistance) {
    if (m_physicsSystem) {
        return m_physicsSystem->Raycast(origin, direction, maxDistance);
    }
    return PhysicsSystem::RaycastResult{};
}

void PhysicsSubsystem::CreateTestBodies() {
    Logger::Info("PhysicsSubsystem", "Creating test physics bodies...");

    // Ground plane (static)
    CreateBox(Vector3(0, -1, 0), Vector3(50, 1, 50), MotionType::Static);

    // Falling boxes
    for (int i = 0; i < 5; ++i) {
        CreateBox(Vector3(i * 2.0f - 4.0f, 10 + i * 2, 0), Vector3(0.5f), MotionType::Dynamic);
    }

    // Falling spheres  
    for (int i = 0; i < 3; ++i) {
        CreateSphere(Vector3(i * 1.5f - 1.5f, 15, 5), 0.5f, MotionType::Dynamic);
    }

    Logger::Info("PhysicsSubsystem", "Test bodies created");
}

} // namespace AstralEngine
```

---

## 🧪 Test ve Doğrulama

### 1. Physics Test Application

Test için `test_physics.cpp` oluşturun:

```cpp
#include "Source/Core/Engine.h"
#include "Source/Subsystems/Platform/PlatformSubsystem.h"
#include "Source/Subsystems/Physics/PhysicsSubsystem.h"

using namespace AstralEngine;

class PhysicsTestApplication {
public:
    bool Run() {
        try {
            Engine engine;
            
            // Subsystem'leri kaydet
            engine.RegisterSubsystem<PlatformSubsystem>();
            engine.RegisterSubsystem<PhysicsSubsystem>();
            
            Logger::Info("PhysicsTest", "Starting Jolt Physics integration test...");
            
            // Engine'i başlat
            engine.Run();
            
            Logger::Info("PhysicsTest", "Physics test completed successfully!");
            return true;
            
        } catch (const std::exception& e) {
            Logger::Error("PhysicsTest", "Physics test failed: {}", e.what());
            return false;
        }
    }
};

int main() {
    PhysicsTestApplication app;
    return app.Run() ? 0 : -1;
}
```

### 2. Unit Test - Raycast Test

`Tests/PhysicsTests.cpp`:

```cpp
#include <gtest/gtest.h>
#include "Source/Physics/PhysicsSystem.h"

using namespace AstralEngine;

class PhysicsSystemTest : public ::testing::Test {
protected:
    void SetUp() override {
        m_physicsSystem = std::make_unique<PhysicsSystem>();
        ASSERT_TRUE(m_physicsSystem->Initialize());
    }

    void TearDown() override {
        m_physicsSystem->Shutdown();
        m_physicsSystem.reset();
    }

    std::unique_ptr<PhysicsSystem> m_physicsSystem;
};

TEST_F(PhysicsSystemTest, CreateAndDestroyBody) {
    // Box body oluştur
    PhysicsBodyID bodyID = m_physicsSystem->CreateBody(
        Vector3(0, 5, 0), 
        Quaternion::Identity, 
        MotionType::Dynamic,
        PhysicsShapeType::Box,
        Vector3(1, 1, 1)
    );
    
    EXPECT_TRUE(bodyID.IsValid());
    
    // Position doğrula
    Vector3 position = m_physicsSystem->GetBodyPosition(bodyID);
    EXPECT_NEAR(position.y, 5.0f, 0.01f);
    
    // Body'i yok et
    m_physicsSystem->DestroyBody(bodyID);
}

TEST_F(PhysicsSystemTest, RaycastTest) {
    // Ground body oluştur
    PhysicsBodyID groundID = m_physicsSystem->CreateBody(
        Vector3(0, 0, 0),
        Quaternion::Identity,
        MotionType::Static,
        PhysicsShapeType::Box,
        Vector3(10, 1, 10)
    );
    
    // Raycast yap
    auto result = m_physicsSystem->Raycast(
        Vector3(0, 10, 0),  // Origin
        Vector3(0, -1, 0),  // Direction (down)
        20.0f               // Max distance
    );
    
    EXPECT_TRUE(result.hit);
    EXPECT_EQ(result.bodyID, groundID);
    EXPECT_NEAR(result.hitPoint.y, 1.0f, 0.01f); // Ground top surface
    
    m_physicsSystem->DestroyBody(groundID);
}

TEST_F(PhysicsSystemTest, BodySimulation) {
    // Falling sphere oluştur
    PhysicsBodyID sphereID = m_physicsSystem->CreateBody(
        Vector3(0, 10, 0),
        Quaternion::Identity,
        MotionType::Dynamic,
        PhysicsShapeType::Sphere,
        Vector3(0.5f)
    );
    
    Vector3 initialPos = m_physicsSystem->GetBodyPosition(sphereID);
    
    // Physics'i birkaç step simule et
    for (int i = 0; i < 30; ++i) {
        m_physicsSystem->Update(1.0f / 60.0f); // 60 FPS
    }
    
    Vector3 finalPos = m_physicsSystem->GetBodyPosition(sphereID);
    
    // Sphere gravity nedeniyle aşağı düşmüş olmalı
    EXPECT_LT(finalPos.y, initialPos.y);
    
    m_physicsSystem->DestroyBody(sphereID);
}
```

### 3. Build ve Test

```bash
# Build
mkdir build && cd build
cmake .. -DUSE_JOLT_PHYSICS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build .

# Test çalıştır
./AstralEngine

# Unit tests
./PhysicsTests

# Log çıktısında arayın:
# [INFO] [PhysicsSubsystem] Jolt Physics initialized successfully
# [INFO] [PhysicsSubsystem] Version: 5.3.0
# [DEBUG] [PhysicsSystem] Created body with ID: 1
```

---

## 🚀 Advanced Features

Bu temel entegrasyon tamamlandıktan sonra ekleyebileceğiniz gelişmiş özellikler:

### 1. **Soft Body Physics**
- Cloth simulation
- Deformable objects
- Soft body skinning

### 2. **Character Controllers** 
- Virtual character movement
- Rigid body character
- Smooth character collision

### 3. **Vehicle Physics**
- Wheeled vehicles
- Tracked vehicles  
- Motorcycles

### 4. **Constraint System**
- Fixed joints
- Hinge joints
- Distance constraints
- 6DOF constraints

### 5. **Performance Optimizations**
- Multi-threading
- Broad phase optimization
- Memory pooling
- SIMD optimizations

---

## 📚 Yararlı Linkler

- **Jolt Physics GitHub:** https://github.com/jrouwe/JoltPhysics
- **Jolt Physics Documentation:** https://jrouwe.github.io/JoltPhysics/
- **GDC 2022 Talk:** [Architecting Jolt Physics for 'Horizon Forbidden West'](https://gdcvault.com/play/1027560/Architecting-Jolt-Physics-for-Horizon)
- **HelloWorld CMake Örneği:** https://github.com/jrouwe/JoltPhysicsHelloWorld
- **Samples ve Testler:** https://github.com/jrouwe/JoltPhysics/tree/master/Samples

Bu rehber, Astral Engine'e Jolt Physics'i tam ve güvenli bir şekilde entegre etmeniz için gerekli tüm adımları içermektedir. Modern physics simulation özellikleriyle AAA kalitesinde oyunlar geliştirebilirsiniz!
