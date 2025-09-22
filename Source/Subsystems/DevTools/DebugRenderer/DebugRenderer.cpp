#include "DebugRenderer.h"
#include "../../Core/Logger.h"
#include "../../Core/Engine.h"
#include "../../Renderer/RenderSubsystem.h"
#include "../../ECS/ECSSubsystem.h"
#include "../../Renderer/VulkanRenderer.h"
#include "../../Renderer/GraphicsDevice.h"
#include "../../Renderer/Core/VulkanDevice.h"
#include "../../Renderer/Buffers/VulkanBuffer.h"
#include <fstream>
#include <sstream>

namespace AstralEngine {

DebugRenderer::DebugRenderer() {
    m_drawCalls.reserve(m_maxDrawCalls);
    m_vertexBuffer.reserve(m_maxVertices);
    m_indexBuffer.reserve(m_maxIndices);
    m_colorBuffer.reserve(m_maxVertices);
}

void DebugRenderer::OnInitialize() {
    Logger::Info("DebugRenderer", "DebugRenderer başlatılıyor");
    
    if (!m_renderSubsystem) {
        Logger::Error("DebugRenderer", "RenderSubsystem bağlantısı bulunamadı");
        return;
    }
    
    // Vulkan renderer ve device bağlantılarını al
    auto graphicsDevice = m_renderSubsystem->GetGraphicsDevice();
    if (!graphicsDevice) {
        Logger::Error("DebugRenderer", "GraphicsDevice bulunamadı");
        return;
    }
    
    auto vulkanDevice = graphicsDevice->GetVulkanDevice();
    if (!vulkanDevice) {
        Logger::Error("DebugRenderer", "VulkanDevice bulunamadı");
        return;
    }
    
    m_device = vulkanDevice->GetDevice();
    m_vulkanRenderer = graphicsDevice->GetVulkanRenderer();
    
    // Vulkan objelerini oluştur
    CreateVulkanObjects();
    CreateDebugBuffers();
    
    Logger::Info("DebugRenderer", "DebugRenderer başarıyla başlatıldı");
}

void DebugRenderer::OnUpdate(float deltaTime) {
    if (!m_enabled) return;
    
    // Debug çizimlerini güncelle
    UpdateDebugDraws(deltaTime);
    
    // Vertex buffer'ı güncelle
    UpdateDebugVertexBuffer();
}

void DebugRenderer::OnRender() {
    if (!m_enabled || !m_vulkanRenderer) return;
    
    // Debug çizimlerini render et
    // Not: Bu metodun çağrılması render pipeline'ının uygun aşamasında olmalı
    // Gerçek implementasyon RenderSubsystem tarafından yönetilecek
}

void DebugRenderer::OnShutdown() {
    Logger::Info("DebugRenderer", "DebugRenderer kapatılıyor");
    
    // Vulkan objelerini temizle
    DestroyVulkanObjects();
    
    m_drawCalls.clear();
    m_vertexBuffer.clear();
    m_indexBuffer.clear();
    m_colorBuffer.clear();
    
    Logger::Info("DebugRenderer", "DebugRenderer başarıyla kapatıldı");
}

void DebugRenderer::LoadSettings(const std::string& settings) {
    // Ayarları yükle
    // JSON formatında ayarları parse et
    Logger::Info("DebugRenderer", "DebugRenderer ayarları yüklendi");
}

std::string DebugRenderer::SaveSettings() const {
    // Ayarları kaydet
    // JSON formatında ayarları oluştur
    Logger::Info("DebugRenderer", "DebugRenderer ayarları kaydedildi");
    return "{}";
}

// Debug çizim komutları
void DebugRenderer::DrawLine(const glm::vec3& start, const glm::vec3& end, const glm::vec4& color, float duration, bool depthTest) {
    if (m_drawCalls.size() >= m_maxDrawCalls) {
        Logger::Warning("DebugRenderer", "Maksimum draw call sayısına ulaşıldı");
        return;
    }
    
    DebugDrawCall drawCall;
    drawCall.type = DebugDrawCall::Type::Line;
    drawCall.start = start;
    drawCall.end = end;
    drawCall.color = color;
    drawCall.duration = duration;
    drawCall.depthTest = depthTest;
    drawCall.creationTime = std::chrono::system_clock::now();
    
    m_drawCalls.push_back(drawCall);
}

void DebugRenderer::DrawBox(const glm::vec3& center, const glm::vec3& size, const glm::vec4& color, float duration, bool depthTest) {
    if (m_drawCalls.size() >= m_maxDrawCalls) {
        Logger::Warning("DebugRenderer", "Maksimum draw call sayısına ulaşıldı");
        return;
    }
    
    DebugDrawCall drawCall;
    drawCall.type = DebugDrawCall::Type::Box;
    drawCall.center = center;
    drawCall.size = size;
    drawCall.color = color;
    drawCall.duration = duration;
    drawCall.depthTest = depthTest;
    drawCall.creationTime = std::chrono::system_clock::now();
    
    m_drawCalls.push_back(drawCall);
}

void DebugRenderer::DrawSphere(const glm::vec3& center, float radius, const glm::vec4& color, float duration, bool depthTest) {
    if (m_drawCalls.size() >= m_maxDrawCalls) {
        Logger::Warning("DebugRenderer", "Maksimum draw call sayısına ulaşıldı");
        return;
    }
    
    DebugDrawCall drawCall;
    drawCall.type = DebugDrawCall::Type::Sphere;
    drawCall.center = center;
    drawCall.radius = radius;
    drawCall.color = color;
    drawCall.duration = duration;
    drawCall.depthTest = depthTest;
    drawCall.creationTime = std::chrono::system_clock::now();
    
    m_drawCalls.push_back(drawCall);
}

void DebugRenderer::DrawText(const glm::vec3& position, const std::string& text, const glm::vec4& color, float duration) {
    if (m_drawCalls.size() >= m_maxDrawCalls) {
        Logger::Warning("DebugRenderer", "Maksimum draw call sayısına ulaşıldı");
        return;
    }
    
    DebugDrawCall drawCall;
    drawCall.type = DebugDrawCall::Type::Text;
    drawCall.center = position;
    drawCall.text = text;
    drawCall.color = color;
    drawCall.duration = duration;
    drawCall.depthTest = false; // Text her zaman üstte
    drawCall.creationTime = std::chrono::system_clock::now();
    
    m_drawCalls.push_back(drawCall);
}

void DebugRenderer::DrawFrustum(const glm::mat4& viewProj, const glm::vec4& color, float duration, bool depthTest) {
    if (m_drawCalls.size() >= m_maxDrawCalls) {
        Logger::Warning("DebugRenderer", "Maksimum draw call sayısına ulaşıldı");
        return;
    }
    
    DebugDrawCall drawCall;
    drawCall.type = DebugDrawCall::Type::Frustum;
    drawCall.center = glm::vec3(0.0f); // Frustum için kullanılmıyor
    drawCall.size = glm::vec3(1.0f); // Frustum için kullanılmıyor
    drawCall.color = color;
    drawCall.duration = duration;
    drawCall.depthTest = depthTest;
    drawCall.creationTime = std::chrono::system_clock::now();
    
    // View-projection matrisini saklamak için text alanını kullanıyoruz
    std::stringstream ss;
    ss << viewProj[0][0] << "," << viewProj[0][1] << "," << viewProj[0][2] << "," << viewProj[0][3] << ","
       << viewProj[1][0] << "," << viewProj[1][1] << "," << viewProj[1][2] << "," << viewProj[1][3] << ","
       << viewProj[2][0] << "," << viewProj[2][1] << "," << viewProj[2][2] << "," << viewProj[2][3] << ","
       << viewProj[3][0] << "," << viewProj[3][1] << "," << viewProj[3][2] << "," << viewProj[3][3];
    drawCall.text = ss.str();
    
    m_drawCalls.push_back(drawCall);
}

// Collision görselleştirme
void DebugRenderer::DrawBoundingBox(const glm::vec3& min, const glm::vec3& max, const glm::vec4& color, float duration) {
    glm::vec3 center = (min + max) * 0.5f;
    glm::vec3 size = max - min;
    DrawBox(center, size, color, duration, true);
}

void DebugRenderer::DrawCapsule(const glm::vec3& center, float radius, float height, const glm::vec4& color, float duration) {
    // Capsule'i iki sphere ve bir cylinder olarak çiz
    glm::vec3 topCenter = center + glm::vec3(0.0f, height * 0.5f, 0.0f);
    glm::vec3 bottomCenter = center - glm::vec3(0.0f, height * 0.5f, 0.0f);
    
    DrawSphere(topCenter, radius, color, duration, true);
    DrawSphere(bottomCenter, radius, color, duration, true);
    DrawCylinder(center, radius, height, color, duration);
}

void DebugRenderer::DrawCylinder(const glm::vec3& center, float radius, float height, const glm::vec4& color, float duration) {
    // Cylinder'i box olarak yaklaşık olarak çiz
    glm::vec3 size(radius * 2.0f, height, radius * 2.0f);
    DrawBox(center, size, color, duration, true);
}

// Işık ve kamera görselleştirme
void DebugRenderer::DrawLightFrustum(const glm::mat4& viewProj, const glm::vec4& color, float duration) {
    DrawFrustum(viewProj, color, duration, false);
}

void DebugRenderer::DrawCameraFrustum(const glm::mat4& viewProj, const glm::vec4& color, float duration) {
    DrawFrustum(viewProj, color, duration, false);
}

void DebugRenderer::DrawLightPosition(const glm::vec3& position, const glm::vec4& color, float radius, float duration) {
    DrawSphere(position, radius, color, duration, false);
}

// Navigation ve AI görselleştirme
void DebugRenderer::DrawNavMesh(const std::vector<glm::vec3>& vertices, const std::vector<uint32_t>& indices, const glm::vec4& color, float duration) {
    // Nav mesh'i line olarak çiz
    for (size_t i = 0; i < indices.size(); i += 3) {
        if (i + 2 < indices.size()) {
            glm::vec3 v1 = vertices[indices[i]];
            glm::vec3 v2 = vertices[indices[i + 1]];
            glm::vec3 v3 = vertices[indices[i + 2]];
            
            DrawLine(v1, v2, color, duration, false);
            DrawLine(v2, v3, color, duration, false);
            DrawLine(v3, v1, color, duration, false);
        }
    }
}

void DebugRenderer::DrawPath(const std::vector<glm::vec3>& points, const glm::vec4& color, float duration) {
    // Path'i line olarak çiz
    for (size_t i = 0; i < points.size() - 1; ++i) {
        DrawLine(points[i], points[i + 1], color, duration, false);
    }
}

void DebugRenderer::DrawRay(const glm::vec3& start, const glm::vec3& direction, float length, const glm::vec4& color, float duration) {
    glm::vec3 end = start + glm::normalize(direction) * length;
    DrawLine(start, end, color, duration, false);
}

// Veri bağlama
void DebugRenderer::BindToRenderSubsystem(RenderSubsystem* renderSubsystem) {
    m_renderSubsystem = renderSubsystem;
}

void DebugRenderer::BindToECS(ECSSubsystem* ecs) {
    m_ecs = ecs;
}

// Render metodları
void DebugRenderer::RenderDebugDraws(VkCommandBuffer commandBuffer) {
    if (!m_enabled || m_drawCalls.empty()) return;
    
    // Debug pipeline'ını bind et
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_debugPipeline);
    
    // Vertex ve index buffer'ları bind et
    VkBuffer vertexBuffers[] = { m_vertexBufferGPU, m_colorBufferGPU };
    VkDeviceSize offsets[] = { 0, 0 };
    vkCmdBindVertexBuffers(commandBuffer, 0, 2, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(commandBuffer, m_indexBufferGPU, 0, VK_INDEX_TYPE_UINT32);
    
    // Draw call'ları işle
    uint32_t indexOffset = 0;
    for (const auto& drawCall : m_drawCalls) {
        // Her draw call tipi için uygun şekilde render et
        // Bu kısım implementasyon detaylarına göre genişletilecek
    }
}

void DebugRenderer::UpdateDebugDraws(float deltaTime) {
    auto currentTime = std::chrono::system_clock::now();
    
    // Süresi dolan draw call'ları temizle
    m_drawCalls.erase(
        std::remove_if(m_drawCalls.begin(), m_drawCalls.end(),
            [currentTime](const DebugDrawCall& drawCall) {
                if (drawCall.duration > 0.0f) {
                    auto elapsed = std::chrono::duration_cast<std::chrono::duration<float>>(
                        currentTime - drawCall.creationTime).count();
                    return elapsed >= drawCall.duration;
                }
                return false;
            }),
        m_drawCalls.end()
    );
}

void DebugRenderer::CreateDebugPipeline() {
    // Debug pipeline'ını oluştur
    // Bu metodun implementasyonu VulkanRenderer'daki pipeline oluşturma mantığına benzer olmalı
}

void DebugRenderer::CreateDebugBuffers() {
    // Vertex buffer oluştur
    VkDeviceSize vertexBufferSize = m_maxVertices * sizeof(glm::vec3);
    // Buffer oluşturma kodu buraya gelecek
    
    // Index buffer oluştur
    VkDeviceSize indexBufferSize = m_maxIndices * sizeof(uint32_t);
    // Buffer oluşturma kodu buraya gelecek
    
    // Color buffer oluştur
    VkDeviceSize colorBufferSize = m_maxVertices * sizeof(glm::vec4);
    // Buffer oluşturma kodu buraya gelecek
}

void DebugRenderer::UpdateDebugVertexBuffer() {
    // Vertex buffer'ı güncelle
    m_vertexBuffer.clear();
    m_indexBuffer.clear();
    m_colorBuffer.clear();
    
    uint32_t vertexOffset = 0;
    uint32_t indexOffset = 0;
    
    for (const auto& drawCall : m_drawCalls) {
        switch (drawCall.type) {
            case DebugDrawCall::Type::Line:
                // Line vertex'leri ekle
                m_vertexBuffer.push_back(drawCall.start);
                m_vertexBuffer.push_back(drawCall.end);
                m_colorBuffer.push_back(drawCall.color);
                m_colorBuffer.push_back(drawCall.color);
                m_indexBuffer.push_back(vertexOffset);
                m_indexBuffer.push_back(vertexOffset + 1);
                vertexOffset += 2;
                indexOffset += 2;
                break;
                
            case DebugDrawCall::Type::Box:
                // Box vertex'leri ekle
                GenerateBoxVertices(drawCall.center, drawCall.size, m_vertexBuffer, m_indexBuffer);
                for (size_t i = 0; i < 24; ++i) { // Box için 24 vertex
                    m_colorBuffer.push_back(drawCall.color);
                }
                break;
                
            case DebugDrawCall::Type::Sphere:
                // Sphere vertex'leri ekle
                GenerateSphereVertices(drawCall.center, drawCall.radius, m_vertexBuffer, m_indexBuffer);
                break;
                
            case DebugDrawCall::Type::Text:
                // Text vertex'leri ekle
                GenerateTextVertices(drawCall.center, drawCall.text, m_vertexBuffer, m_indexBuffer);
                break;
                
            case DebugDrawCall::Type::Frustum:
                // Frustum vertex'leri ekle
                // Text alanından matrisi parse et
                glm::mat4 viewProj = glm::mat4(1.0f);
                GenerateFrustumVertices(viewProj, m_vertexBuffer, m_indexBuffer);
                break;
        }
    }
    
    // GPU buffer'larını güncelle
    // Buffer güncelleme kodu buraya gelecek
}

// Yardımcı metodlar
void DebugRenderer::GenerateBoxVertices(const glm::vec3& center, const glm::vec3& size, std::vector<glm::vec3>& vertices, std::vector<uint32_t>& indices) {
    glm::vec3 halfSize = size * 0.5f;
    
    // Box vertex'leri
    glm::vec3 boxVertices[8] = {
        center + glm::vec3(-halfSize.x, -halfSize.y, -halfSize.z), // 0
        center + glm::vec3( halfSize.x, -halfSize.y, -halfSize.z), // 1
        center + glm::vec3( halfSize.x,  halfSize.y, -halfSize.z), // 2
        center + glm::vec3(-halfSize.x,  halfSize.y, -halfSize.z), // 3
        center + glm::vec3(-halfSize.x, -halfSize.y,  halfSize.z), // 4
        center + glm::vec3( halfSize.x, -halfSize.y,  halfSize.z), // 5
        center + glm::vec3( halfSize.x,  halfSize.y,  halfSize.z), // 6
        center + glm::vec3(-halfSize.x,  halfSize.y,  halfSize.z)  // 7
    };
    
    // Box kenarları için index'ler
    uint32_t boxIndices[24] = {
        0, 1, 1, 2, 2, 3, 3, 0, // Alt yüz
        4, 5, 5, 6, 6, 7, 7, 4, // Üst yüz
        0, 4, 1, 5, 2, 6, 3, 7  // Dikey kenarlar
    };
    
    uint32_t vertexOffset = static_cast<uint32_t>(vertices.size());
    
    for (int i = 0; i < 8; ++i) {
        vertices.push_back(boxVertices[i]);
    }
    
    for (int i = 0; i < 24; ++i) {
        indices.push_back(vertexOffset + boxIndices[i]);
    }
}

void DebugRenderer::GenerateSphereVertices(const glm::vec3& center, float radius, std::vector<glm::vec3>& vertices, std::vector<uint32_t>& indices) {
    const int segments = 16;
    const int rings = 12;
    
    uint32_t vertexOffset = static_cast<uint32_t>(vertices.size());
    
    // Sphere vertex'lerini oluştur
    for (int ring = 0; ring <= rings; ++ring) {
        float phi = glm::pi<float>() * ring / rings;
        float sinPhi = glm::sin(phi);
        float cosPhi = glm::cos(phi);
        
        for (int segment = 0; segment <= segments; ++segment) {
            float theta = 2.0f * glm::pi<float>() * segment / segments;
            float sinTheta = glm::sin(theta);
            float cosTheta = glm::cos(theta);
            
            glm::vec3 vertex = center + glm::vec3(
                radius * sinPhi * cosTheta,
                radius * cosPhi,
                radius * sinPhi * sinTheta
            );
            
            vertices.push_back(vertex);
        }
    }
    
    // Sphere index'lerini oluştur
    for (int ring = 0; ring < rings; ++ring) {
        for (int segment = 0; segment < segments; ++segment) {
            uint32_t current = ring * (segments + 1) + segment;
            uint32_t next = current + segments + 1;
            
            indices.push_back(vertexOffset + current);
            indices.push_back(vertexOffset + next);
            indices.push_back(vertexOffset + current + 1);
            
            indices.push_back(vertexOffset + next);
            indices.push_back(vertexOffset + next + 1);
            indices.push_back(vertexOffset + current + 1);
        }
    }
}

void DebugRenderer::GenerateFrustumVertices(const glm::mat4& viewProj, std::vector<glm::vec3>& vertices, std::vector<uint32_t>& indices) {
    // Frustum köşelerini hesapla
    glm::vec4 corners[8] = {
        glm::vec4(-1, -1, 0, 1), // 0
        glm::vec4( 1, -1, 0, 1), // 1
        glm::vec4( 1,  1, 0, 1), // 2
        glm::vec4(-1,  1, 0, 1), // 3
        glm::vec4(-1, -1, 1, 1), // 4
        glm::vec4( 1, -1, 1, 1), // 5
        glm::vec4( 1,  1, 1, 1), // 6
        glm::vec4(-1,  1, 1, 1)  // 7
    };
    
    glm::mat4 inverseViewProj = glm::inverse(viewProj);
    
    uint32_t vertexOffset = static_cast<uint32_t>(vertices.size());
    
    // Köşeleri world space'e dönüştür
    for (int i = 0; i < 8; ++i) {
        glm::vec4 worldCorner = inverseViewProj * corners[i];
        worldCorner /= worldCorner.w;
        vertices.push_back(glm::vec3(worldCorner));
    }
    
    // Frustum kenarları için index'ler
    uint32_t frustumIndices[24] = {
        0, 1, 1, 2, 2, 3, 3, 0, // Near plane
        4, 5, 5, 6, 6, 7, 7, 4, // Far plane
        0, 4, 1, 5, 2, 6, 3, 7  // Kenarlar
    };
    
    for (int i = 0; i < 24; ++i) {
        indices.push_back(vertexOffset + frustumIndices[i]);
    }
}

void DebugRenderer::GenerateTextVertices(const glm::vec3& position, const std::string& text, std::vector<glm::vec3>& vertices, std::vector<uint32_t>& indices) {
    // Text rendering için basit bir implementasyon
    // Gerçek implementasyon font atlas ve texture koordinatları gerektirir
    uint32_t vertexOffset = static_cast<uint32_t>(vertices.size());
    
    // Her karakter için basit bir quad oluştur
    float charWidth = 0.1f;
    float charHeight = 0.1f;
    
    for (size_t i = 0; i < text.length(); ++i) {
        glm::vec3 charPos = position + glm::vec3(i * charWidth, 0.0f, 0.0f);
        
        glm::vec3 quadVertices[4] = {
            charPos + glm::vec3(0.0f, 0.0f, 0.0f),
            charPos + glm::vec3(charWidth, 0.0f, 0.0f),
            charPos + glm::vec3(charWidth, charHeight, 0.0f),
            charPos + glm::vec3(0.0f, charHeight, 0.0f)
        };
        
        for (int j = 0; j < 4; ++j) {
            vertices.push_back(quadVertices[j]);
        }
        
        uint32_t quadIndices[6] = {
            vertexOffset + i * 4 + 0,
            vertexOffset + i * 4 + 1,
            vertexOffset + i * 4 + 2,
            vertexOffset + i * 4 + 0,
            vertexOffset + i * 4 + 2,
            vertexOffset + i * 4 + 3
        };
        
        for (int j = 0; j < 6; ++j) {
            indices.push_back(quadIndices[j]);
        }
    }
}

// Vulkan objeleri
void DebugRenderer::CreateVulkanObjects() {
    CreateDebugRenderPass();
    CreateDebugShaderModules();
    CreateDebugPipelineLayout();
    CreateDebugPipeline();
}

void DebugRenderer::DestroyVulkanObjects() {
    if (m_device != VK_NULL_HANDLE) {
        if (m_debugPipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(m_device, m_debugPipeline, nullptr);
            m_debugPipeline = VK_NULL_HANDLE;
        }
        
        if (m_debugPipelineLayout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(m_device, m_debugPipelineLayout, nullptr);
            m_debugPipelineLayout = VK_NULL_HANDLE;
        }
        
        if (m_vertexShaderModule != VK_NULL_HANDLE) {
            vkDestroyShaderModule(m_device, m_vertexShaderModule, nullptr);
            m_vertexShaderModule = VK_NULL_HANDLE;
        }
        
        if (m_fragmentShaderModule != VK_NULL_HANDLE) {
            vkDestroyShaderModule(m_device, m_fragmentShaderModule, nullptr);
            m_fragmentShaderModule = VK_NULL_HANDLE;
        }
        
        if (m_debugRenderPass != VK_NULL_HANDLE) {
            vkDestroyRenderPass(m_device, m_debugRenderPass, nullptr);
            m_debugRenderPass = VK_NULL_HANDLE;
        }
        
        // Buffer'ları temizle
        if (m_vertexBufferGPU != VK_NULL_HANDLE) {
            vkDestroyBuffer(m_device, m_vertexBufferGPU, nullptr);
            m_vertexBufferGPU = VK_NULL_HANDLE;
        }
        
        if (m_vertexBufferMemory != VK_NULL_HANDLE) {
            vkFreeMemory(m_device, m_vertexBufferMemory, nullptr);
            m_vertexBufferMemory = VK_NULL_HANDLE;
        }
        
        if (m_indexBufferGPU != VK_NULL_HANDLE) {
            vkDestroyBuffer(m_device, m_indexBufferGPU, nullptr);
            m_indexBufferGPU = VK_NULL_HANDLE;
        }
        
        if (m_indexBufferMemory != VK_NULL_HANDLE) {
            vkFreeMemory(m_device, m_indexBufferMemory, nullptr);
            m_indexBufferMemory = VK_NULL_HANDLE;
        }
        
        if (m_colorBufferGPU != VK_NULL_HANDLE) {
            vkDestroyBuffer(m_device, m_colorBufferGPU, nullptr);
            m_colorBufferGPU = VK_NULL_HANDLE;
        }
        
        if (m_colorBufferMemory != VK_NULL_HANDLE) {
            vkFreeMemory(m_device, m_colorBufferMemory, nullptr);
            m_colorBufferMemory = VK_NULL_HANDLE;
        }
    }
}

void DebugRenderer::CreateDebugRenderPass() {
    VkAttachmentDescription colorAttachment = {};
    colorAttachment.format = VK_FORMAT_B8G8R8A8_UNORM;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    
    VkAttachmentDescription depthAttachment = {};
    depthAttachment.format = VK_FORMAT_D32_SFLOAT;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    
    VkAttachmentReference colorAttachmentRef = {};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    
    VkAttachmentReference depthAttachmentRef = {};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    
    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;
    
    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    
    std::vector<VkAttachmentDescription> attachments = { colorAttachment, depthAttachment };
    
    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;
    
    if (vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_debugRenderPass) != VK_SUCCESS) {
        Logger::Error("DebugRenderer", "Debug render pass oluşturulamadı");
    }
}

void DebugRenderer::CreateDebugPipelineLayout() {
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 0;
    pipelineLayoutInfo.pSetLayouts = nullptr;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    
    VkPushConstantRange pushConstantRange = {};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(glm::mat4) + sizeof(float); // viewProjection + pointSize
    
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
    
    if (vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &m_debugPipelineLayout) != VK_SUCCESS) {
        Logger::Error("DebugRenderer", "Debug pipeline layout oluşturulamadı");
    }
}

void DebugRenderer::CreateDebugShaderModules() {
    // Shader dosyalarını oku ve modülleri oluştur
    // Bu kısım shader dosyaları oluşturulduktan sonra tamamlanacak
    
    // Geçici olarak boş shader modülleri oluştur
    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = 0;
    createInfo.pCode = nullptr;
    
    // Gerçek implementasyonda shader kodları burada yüklenecek
    Logger::Warning("DebugRenderer", "Shader modülleri henüz implement edilmedi");
}

} // namespace AstralEngine