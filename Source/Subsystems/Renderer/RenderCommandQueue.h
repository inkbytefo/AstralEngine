#pragma once

#include "IRenderer.h"
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>

namespace AstralEngine {

/**
 * @class RenderCommandQueue
 * @brief Thread-safe render komut kuyruğu
 * 
 * Bu sınıf, oyun mantığı thread'i ve render thread'i arasında
 * güvenli bir şekilde komut iletimi sağlar. Double buffering
 * tekniği kullanır performans için.
 */
class RenderCommandQueue {
public:
    RenderCommandQueue();
    ~RenderCommandQueue();

    // Komut ekleme (thread-safe)
    void Push(const RenderCommand& command);
    void Push(const std::vector<RenderCommand>& commands);
    
    // Komutları işleme (sadece render thread'i tarafından çağrılır)
    std::vector<RenderCommand> Swap();
    
    // Kuyruk durumunu sorgulama
    bool IsEmpty() const;
    size_t GetCommandCount() const;
    
    // Kuyruğu temizleme
    void Clear();
    
    // Kuyruk kapasitesi ayarlama
    void SetMaxCommands(size_t maxCommands);

private:
    // Double buffering için iki kuyruk
    std::vector<RenderCommand> m_currentQueue;  // Render thread'i tarafından işleniyor
    std::vector<RenderCommand> m_nextQueue;     // Yeni komutlar buraya ekleniyor
    
    // Thread senkronizasyonu
    mutable std::mutex m_mutex;
    std::condition_variable m_condition;
    std::atomic<bool> m_hasNewCommands;
    
    // Yapılandırma
    size_t m_maxCommands;
    
    // Yardımcı metodlar
    void WaitForCommands();
    void SwapInternal();
};

} // namespace AstralEngine
