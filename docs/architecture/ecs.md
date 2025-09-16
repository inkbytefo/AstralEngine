# Astral Engine - ECS (Entity Component System)

Entity Component System (ECS), Astral Engine'de oyun dünyasının durumunu yöneten ve veri odaklı tasarım prensiplerini uygulayan temel mimari desenidir. Bu dokümantasyon, ECS sisteminin nasıl çalıştığını, bileşenlerin nasıl kullanıldığını ve sistemlerin nasıl entegre edildiğini açıklamaktadır.

## 🎯 ECS Mimarisi

### Temel Kavramlar

ECS, üç temel bileşenden oluşur:

1. **Entity**: Oyun dünyasındaki nesneler (oyuncu, düşman, obje vb.)
2. **Component**: Entity'lerin özellikleri (pozisyon, renk, sağlık vb.)
3. **System**: Component'ler üzerinde çalışan mantık sistemleri (hareket, render, fizik vb.)

### Entity Nedir?

Entity, sadece bir kimlik (ID) taşıyan bir nesnedir. Entity'nin davranışları, ona bağlı olan component'ler tarafından tanımlanır:

```cpp
// Entity sadece bir ID taşıyor
using Entity = uint32_t;

class EntityManager {
public:
    Entity CreateEntity() {
        return m_nextEntityId++;
    }
    
    void DestroyEntity(Entity entity) {
        m_activeEntities.erase(entity);
    }
    
    bool IsValid(Entity entity) const {
        return m_activeEntities.find(entity) != m_activeEntities.end();
    }
    
private:
    Entity m_nextEntityId = 1;
    std::unordered_set<Entity> m_activeEntities;
};
```

### Component Nedir?

Component, veri taşıyan basit veri yapılarıdır. Component'ler entity'lerin özelliklerini tanımlar:

```cpp
// Örnek component'ler
struct TransformComponent {
    glm::vec3 position{0.0f};
    glm::vec3 rotation{0.0f};
    glm::vec3 scale{1.0f};
};

struct RenderComponent {
    std::string modelPath;
    std::string materialPath;
    bool visible = true;
};

struct HealthComponent {
    float maxHealth = 100.0f;
    float currentHealth = 100.0f;
    bool isAlive = true;
};
```

### System Nedir?

System, belirli component'lere sahip entity'ler üzerinde çalışan mantık sistemleridir:

```cpp
class MovementSystem {
public:
    void Update(std::vector<Entity>& entities, 
               ComponentStorage<TransformComponent>& transforms,
               ComponentStorage<MovementComponent>& movements) {
        
        for (Entity entity : entities) {
            auto* transform = transforms.GetComponent(entity);
            auto* movement = movements.GetComponent(entity);
            
            if (transform && movement) {
                // Hareket mantığı
                transform->position += movement->velocity * deltaTime;
            }
        }
    }
};
```

## 📋 Mevcut Component'ler

### 1. TransformComponent

**Amaç**: Entity'nin 3D dönüşüm bilgilerini tutmak

```cpp
struct TransformComponent {
    glm::vec3 position{0.0f};    // Pozisyon
    glm::vec3 rotation{0.0f};    // Rotasyon (Euler açıları)
    glm::vec3 scale{1.0f};       // Ölçek
    
    // World matrix hesaplama
    glm::mat4 GetWorldMatrix() const {
        glm::mat4 translation = glm::translate(glm::mat4(1.0f), position);
        glm::mat4 rotX = glm::rotate(glm::mat4(1.0f), rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
        glm::mat4 rotY = glm::rotate(glm::mat4(1.0f), rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 rotZ = glm::rotate(glm::mat4(1.0f), rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
        glm::mat4 scaling = glm::scale(glm::mat4(1.0f), scale);
        
        return translation * rotY * rotX * rotZ * scaling;
    }
};
```

### 2. RenderComponent

**Amaç**: Entity'nin render edilmesi için gereken bilgileri tutmak

```cpp
struct RenderComponent {
    std::string modelPath;       // Model dosya yolu
    std::string materialPath;    // Materyal dosya yolu
    bool visible = true;         // Görünür mü?
    int renderLayer = 0;         // Render katmanı (düşük değerler önce render edilir)
    
    // Render özellikleri
    bool castsShadows = true;    // Gölge atar mı?
    bool receivesShadows = true; // Gölge alır mı?
};
```

### 3. CameraComponent

**Amaç**: Kamera özelliklerini ve projeksiyon matrislerini tutmak

```cpp
struct CameraComponent {
    enum class ProjectionType { Perspective, Orthographic };
    
    ProjectionType projectionType = ProjectionType::Perspective;
    
    // Perspective projection
    float fieldOfView = 45.0f;   // Field of view (derece)
    float nearPlane = 0.1f;      // Yakın kesme düzlemi
    float farPlane = 1000.0f;    // Uzak kesme düzlemi
    
    // Orthographic projection
    float orthoLeft = -10.0f;
    float orthoRight = 10.0f;
    float orthoBottom = -10.0f;
    float orthoTop = 10.0f;
    
    bool isMainCamera = false;   // Ana kamera mı?
    
    // Projection matrix hesaplama
    glm::mat4 GetProjectionMatrix(float aspectRatio) const {
        if (projectionType == ProjectionType::Perspective) {
            return glm::perspective(glm::radians(fieldOfView), aspectRatio, nearPlane, farPlane);
        } else {
            return glm::ortho(orthoLeft, orthoRight, orthoBottom, orthoTop, nearPlane, farPlane);
        }
    }
};
```

### 4. LightComponent

**Amaç**: Işık kaynaklarını tanımlamak

```cpp
struct LightComponent {
    enum class LightType { Directional, Point, Spot };
    
    LightType type = LightType::Point;
    glm::vec3 color{1.0f};       // Işık rengi
    float intensity = 1.0f;      // Işık yoğunluğu
    
    // Point ve Spot light için
    float range = 10.0f;         // Işık menzili
    float constant = 1.0f;       // Sabit terim
    float linear = 0.09f;        // Doğrusal terim
    float quadratic = 0.032f;    // Kareli terim
    
    // Spot light için
    float innerCone = 30.0f;     // İç koni açısı
    float outerCone = 45.0f;     // Dış koni açısı
    
    bool castsShadows = false;   // Gölge atar mı?
};
```

### 5. MovementComponent

**Amaç**: Entity'nin hareket özelliklerini tutmak

```cpp
struct MovementComponent {
    glm::vec3 velocity{0.0f};        // Hız vektörü
    glm::vec3 acceleration{0.0f};    // İvme vektörü
    glm::vec3 angularVelocity{0.0f}; // Açısal hız
    
    float maxSpeed = 10.0f;          // Maksimum hız
    float friction = 0.98f;          // Sürtünme katsayısı (0.0f = yok, 1.0f = anlık durma)
};
```

### 6. Diğer Component'ler

```cpp
struct NameComponent {
    std::string name;    // Entity'nin adı
};

struct TagComponent {
    std::string tag;     // Entity'nin etiketi
};

struct ScriptComponent {
    std::string scriptPath;  // Script dosya yolu
    bool enabled = true;     // Aktif mi?
    void* scriptInstance;    // Script instance (gelecekte)
};

struct HierarchyComponent {
    uint32_t parent = 0;     // Parent entity (0 = yok)
    std::vector<uint32_t> children; // Child entity'ler
};
```

## 🔧 Component Yönetimi

### ComponentStorage

Component'lerin verimli bir şekilde saklandığı ve erişildiği sınıf:

```cpp
template<typename T>
class ComponentStorage {
public:
    T* GetComponent(Entity entity) {
        auto it = m_components.find(entity);
        return it != m_components.end() ? &it->second : nullptr;
    }
    
    T* AddComponent(Entity entity, const T& component = T()) {
        m_components[entity] = component;
        return &m_components[entity];
    }
    
    void RemoveComponent(Entity entity) {
        m_components.erase(entity);
    }
    
    bool HasComponent(Entity entity) const {
        return m_components.find(entity) != m_components.end();
    }
    
    std::vector<Entity> GetEntitiesWithComponent() const {
        std::vector<Entity> entities;
        for (const auto& pair : m_components) {
            entities.push_back(pair.first);
        }
        return entities;
    }
    
private:
    std::unordered_map<Entity, T> m_components;
};
```

### Component Manager

Tüm component depolarını yöneten merkezi sınıf:

```cpp
class ComponentManager {
public:
    template<typename T>
    ComponentStorage<T>* GetComponentStorage() {
        auto it = m_storages.find(typeid(T));
        if (it == m_storages.end()) {
            auto storage = std::make_unique<ComponentStorage<T>>();
            it = m_storages.emplace(typeid(T), std::move(storage)).first;
        }
        return static_cast<ComponentStorage<T>*>(it->second.get());
    }
    
    template<typename T>
    ComponentStorage<T>& GetComponentStorage() {
        return *GetComponentStorage<T>();
    }
    
    void EntityDestroyed(Entity entity) {
        for (auto& pair : m_storages) {
            pair.second->RemoveComponent(entity);
        }
    }
    
private:
    std::unordered_map<std::type_index, std::unique_ptr<IComponentStorage>> m_storages;
};
```

## 🔄 System Entegrasyonu

### System Temel Sınıfı

Tüm sistemlerin uyması gereken temel arayüz:

```cpp
class ISystem {
public:
    virtual ~ISystem() = default;
    
    // System güncelleme
    virtual void Update(float deltaTime) = 0;
    
    // System adı (hata ayıklama için)
    virtual const char* GetName() const = 0;
    
    // Gerekli component'ler
    virtual const std::vector<std::type_index>& GetRequiredComponents() const = 0;
};
```

### Örnek System'ler

#### 1. TransformSystem

```cpp
class TransformSystem : public ISystem {
public:
    void Update(float deltaTime) override {
        auto& transforms = m_componentManager.GetComponentStorage<TransformComponent>();
        auto& movements = m_componentManager.GetComponentStorage<MovementComponent>();
        
        for (Entity entity : m_entities) {
            auto* transform = transforms.GetComponent(entity);
            auto* movement = movements.GetComponent(entity);
            
            if (transform && movement) {
                // Hareket güncelleme
                transform->position += movement->velocity * deltaTime;
                
                // Sürtünme uygula
                movement->velocity *= movement->friction;
            }
        }
    }
    
    const char* GetName() const override { return "TransformSystem"; }
    
    const std::vector<std::type_index>& GetRequiredComponents() const override {
        static std::vector<std::type_index> types = { typeid(TransformComponent), typeid(MovementComponent) };
        return types;
    }
    
private:
    ComponentManager& m_componentManager;
    std::vector<Entity> m_entities;
};
```

#### 2. RenderSystem

```cpp
class RenderSystem : public ISystem {
public:
    void Update(float deltaTime) override {
        auto& transforms = m_componentManager.GetComponentStorage<TransformComponent>();
        auto& renders = m_componentManager.GetComponentStorage<RenderComponent>();
        
        // Render edilebilir entity'leri topla
        std::vector<RenderData> renderData;
        for (Entity entity : m_entities) {
            auto* transform = transforms.GetComponent(entity);
            auto* render = renders.GetComponent(entity);
            
            if (transform && render && render->visible) {
                RenderData data;
                data.entity = entity;
                data.transformMatrix = transform->GetWorldMatrix();
                data.modelPath = render->modelPath;
                data.materialPath = render->materialPath;
                data.renderLayer = render->renderLayer;
                
                renderData.push_back(data);
            }
        }
        
        // Render katmanına göre sırala
        std::sort(renderData.begin(), renderData.end(),
                 [](const RenderData& a, const RenderData& b) {
                     return a.renderLayer < b.renderLayer;
                 });
        
        // Render subsystem'e gönder
        m_renderSubsystem->SubmitRenderData(renderData);
    }
    
    const char* GetName() const override { return "RenderSystem"; }
    
    const std::vector<std::type_index>& GetRequiredComponents() const override {
        static std::vector<std::type_index> types = { typeid(TransformComponent), typeid(RenderComponent) };
        return types;
    }
    
private:
    ComponentManager& m_componentManager;
    RenderSubsystem* m_renderSubsystem;
    std::vector<Entity> m_entities;
};
```

## 🎯 Entity Yönetimi

### Entity Manager

Entity'lerin oluşturulması, yok edilmesi ve sorgulanması:

```cpp
class EntityManager {
public:
    Entity CreateEntity() {
        Entity entity = m_nextEntityId++;
        m_activeEntities.insert(entity);
        return entity;
    }
    
    void DestroyEntity(Entity entity) {
        if (m_activeEntities.erase(entity)) {
            // Component'leri temizle
            m_componentManager.EntityDestroyed(entity);
            
            // Parent-child ilişkilerini güncelle
            UpdateHierarchy(entity);
        }
    }
    
    bool IsValid(Entity entity) const {
        return m_activeEntities.find(entity) != m_activeEntities.end();
    }
    
    std::vector<Entity> GetEntities() const {
        return std::vector<Entity>(m_activeEntities.begin(), m_activeEntities.end());
    }
    
private:
    Entity m_nextEntityId = 1;
    std::unordered_set<Entity> m_activeEntities;
    ComponentManager m_componentManager;
};
```

### Hiyerarşi Yönetimi

Entity'ler arasındaki parent-child ilişkilerini yönetme:

```cpp
void EntityManager::UpdateHierarchy(Entity entity) {
    auto& hierarchy = m_componentManager.GetComponentStorage<HierarchyComponent>();
    auto* parentHierarchy = hierarchy.GetComponent(entity);
    
    if (parentHierarchy) {
        // Parent'ın children listesinden çıkar
        if (parentHierarchy->parent != 0) {
            auto* parent = hierarchy.GetComponent(parentHierarchy->parent);
            if (parent) {
                auto& children = parent->children;
                children.erase(std::remove(children.begin(), children.end(), entity), children.end());
            }
        }
        
        // Entity'nin children'larını yok et
        for (Entity child : parentHierarchy->children) {
            DestroyEntity(child);
        }
    }
}
```

## 📊 System Çalışma Mekanizması

### System Çağrı Sırası

System'lerin çalışma sırası önemlidir. Örneğin, önce hareket sistemi çalışmalı, sonra render sistemi:

```cpp
class ECSSubsystem : public ISubsystem {
public:
    void OnUpdate(float deltaTime) override {
        // System'leri çalıştır (sıralı)
        m_transformSystem->Update(deltaTime);
        m_physicsSystem->Update(deltaTime);
        m_aiSystem->Update(deltaTime);
        m_renderSystem->Update(deltaTime);
    }
    
private:
    std::vector<std::unique_ptr<ISystem>> m_systems;
};
```

### System Filtreleme

System'lerin sadece belirli component'lere sahip entity'ler üzerinde çalışması:

```cpp
class SystemManager {
public:
    template<typename T>
    void RegisterSystem(std::unique_ptr<ISystem> system) {
        m_systems.push_back(std::move(system));
        
        // System'in ihtiyaç duyduğu component'leri bul
        auto& requiredComponents = system->GetRequiredComponents();
        
        // Bu component'lere sahip entity'leri bul
        for (Entity entity : m_entityManager.GetEntities()) {
            bool hasAllComponents = true;
            for (auto& componentType : requiredComponents) {
                if (!m_componentManager.HasComponent(entity, componentType)) {
                    hasAllComponents = false;
                    break;
                }
            }
            
            if (hasAllComponents) {
                system->AddEntity(entity);
            }
        }
    }
    
private:
    std::vector<std::unique_ptr<ISystem>> m_systems;
    EntityManager& m_entityManager;
    ComponentManager& m_componentManager;
};
```

## 🛡️ Bellek Optimizasyonları

### Component Pooling

Component'lerin tekrar kullanımı:

```cpp
template<typename T>
class ComponentPool {
public:
    T* Allocate(Entity entity) {
        if (m_freeIndices.empty()) {
            m_components.push_back(T());
            m_entities.push_back(entity);
            return &m_components.back();
        } else {
            size_t index = m_freeIndices.back();
            m_freeIndices.pop_back();
            m_entities[index] = entity;
            return &m_components[index];
        }
    }
    
    void Deallocate(Entity entity) {
        size_t index = FindIndex(entity);
        if (index != m_components.size()) {
            m_freeIndices.push_back(index);
            m_entities[index] = 0; // Invalid entity
        }
    }
    
private:
    std::vector<T> m_components;
    std::vector<Entity> m_entities;
    std::vector<size_t> m_freeIndices;
};
```

### Cache-Friendly Veri Yapıları

Component'lerin bellekte sürekli olarak depolanması:

```cpp
template<typename T>
class PackedComponentStorage {
public:
    T* GetComponent(Entity entity) {
        auto it = m_entityToIndex.find(entity);
        if (it != m_entityToIndex.end()) {
            return &m_components[it->second];
        }
        return nullptr;
    }
    
    T* AddComponent(Entity entity, const T& component = T()) {
        size_t index = m_components.size();
        m_components.push_back(component);
        m_entities.push_back(entity);
        m_entityToIndex[entity] = index;
        return &m_components[index];
    }
    
    void RemoveComponent(Entity entity) {
        auto it = m_entityToIndex.find(entity);
        if (it != m_entityToIndex.end()) {
            size_t index = it->second;
            size_t lastIndex = m_components.size() - 1;
            
            // Son elemanla değiştir
            if (index != lastIndex) {
                m_components[index] = m_components[lastIndex];
                m_entities[index] = m_entities[lastIndex];
                m_entityToIndex[m_entities[index]] = index;
            }
            
            // Son elemanı sil
            m_components.pop_back();
            m_entities.pop_back();
            m_entityToIndex.erase(entity);
        }
    }
    
private:
    std::vector<T> m_components;
    std::vector<Entity> m_entities;
    std::unordered_map<Entity, size_t> m_entityToIndex;
};
```

## 🔮 Gelecek Geliştirmeler

### 1. Reflection Desteği

Component'lerin runtime'da inspeksiyonu:

```cpp
class ComponentReflection {
public:
    template<typename T>
    static void RegisterComponent() {
        ComponentInfo info;
        info.name = typeid(T).name();
        info.size = sizeof(T);
        info.createFunc = []() { return std::make_unique<T>(); };
        
        m_componentInfos[typeid(T)] = info;
    }
    
    static ComponentInfo GetComponentInfo(std::type_index type) {
        auto it = m_componentInfos.find(type);
        return it != m_componentInfos.end() ? it->second : ComponentInfo();
    }
    
private:
    static std::unordered_map<std::type_index, ComponentInfo> m_componentInfos;
};
```

### 2. Entity-Component-Editor

Görsel component düzenleyici:

```cpp
class ComponentEditor {
public:
    void EditEntity(Entity entity) {
        auto& componentManager = GetComponentManager();
        
        // Tüm component'leri listele
        for (auto& componentType : GetComponentTypes()) {
            if (componentManager.HasComponent(entity, componentType)) {
                EditComponent(entity, componentType);
            }
        }
    }
    
private:
    void EditComponent(Entity entity, std::type_index componentType);
};
```

### 3. System Dependency Graph

System bağımlılıklarının yönetimi:

```cpp
class SystemDependencyGraph {
public:
    void AddSystem(std::unique_ptr<ISystem> system);
    void RemoveSystem(const std::string& systemName);
    void ExecuteSystems(float deltaTime);
    
private:
    std::vector<std::string> GetExecutionOrder();
    
    std::unordered_map<std::string, std::unique_ptr<ISystem>> m_systems;
    std::unordered_map<std::string, std::vector<std::string>> m_dependencies;
};
```

---

ECS sistemi, Astral Engine'de veri odaklı tasarımın temelini oluşturan ve yüksek performanslı oyun mantığı sağlayan kritik bir bileşendir. Bu mimari, oyun dünyasının esnek ve ölçeklenebilir bir şekilde yönetilmesini sağlamaktadır.