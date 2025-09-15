#include "RenderCommandQueue.h"
#include <algorithm>

namespace AstralEngine {

RenderCommandQueue::RenderCommandQueue() 
    : m_hasNewCommands(false)
    , m_maxCommands(10000) { // Varsayılan maksimum komut sayısı
    
    m_currentQueue.reserve(m_maxCommands);
    m_nextQueue.reserve(m_maxCommands);
}

RenderCommandQueue::~RenderCommandQueue() {
    Clear();
}

void RenderCommandQueue::Push(const RenderCommand& command) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_nextQueue.size() >= m_maxCommands) {
        // Kuyruk dolu, eski komutları at
        m_nextQueue.clear();
    }
    
    m_nextQueue.push_back(command);
    m_hasNewCommands.store(true);
    m_condition.notify_one();
}

void RenderCommandQueue::Push(const std::vector<RenderCommand>& commands) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    size_t availableSpace = m_maxCommands - m_nextQueue.size();
    if (commands.size() > availableSpace) {
        // Yeterli alan yok, eski komutları temizle
        m_nextQueue.clear();
    }
    
    m_nextQueue.insert(m_nextQueue.end(), commands.begin(), commands.end());
    m_hasNewCommands.store(true);
    m_condition.notify_one();
}

std::vector<RenderCommand> RenderCommandQueue::Swap() {
    // BEKLEME KISMINI KALDIR - doğrudan işlemeye geç
    // if (!m_hasNewCommands.load()) {
    //     WaitForCommands();
    // }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    SwapInternal();
    
    return std::move(m_currentQueue);
}

bool RenderCommandQueue::IsEmpty() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_currentQueue.empty() && m_nextQueue.empty();
}

size_t RenderCommandQueue::GetCommandCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_currentQueue.size() + m_nextQueue.size();
}

void RenderCommandQueue::Clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_currentQueue.clear();
    m_nextQueue.clear();
    m_hasNewCommands.store(false);
}

void RenderCommandQueue::SetMaxCommands(size_t maxCommands) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_maxCommands = maxCommands;
    
    // Kapasiteyi ayarla
    m_currentQueue.reserve(m_maxCommands);
    m_nextQueue.reserve(m_maxCommands);
    
    // Eğer mevcut komutlar yeni limite aşarsa, temizle
    if (m_currentQueue.size() > m_maxCommands) {
        m_currentQueue.resize(m_maxCommands);
    }
    if (m_nextQueue.size() > m_maxCommands) {
        m_nextQueue.resize(m_maxCommands);
    }
}

void RenderCommandQueue::WaitForCommands() {
    std::unique_lock<std::mutex> lock(m_mutex);
    m_condition.wait(lock, [this] { return m_hasNewCommands.load(); });
}

void RenderCommandQueue::SwapInternal() {
    // Kuyrukları değiştir
    m_currentQueue.clear();
    std::swap(m_currentQueue, m_nextQueue);
    m_hasNewCommands.store(false);
}

} // namespace AstralEngine
