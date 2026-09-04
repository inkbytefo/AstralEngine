#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <memory>
#include <functional>
#include <utility>

namespace Astral {

using EventTypeID = size_t;

namespace Detail {
    inline EventTypeID GetNextEventTypeID() noexcept {
        static EventTypeID s_ID = 0;
        return s_ID++;
    }

    template <typename T>
    inline EventTypeID GetEventTypeID() noexcept {
        static const EventTypeID s_TypeID = GetNextEventTypeID();
        return s_TypeID;
    }

    class IHandlerInvoker {
    public:
        virtual ~IHandlerInvoker() = default;
        [[nodiscard]] virtual uint64_t GetID() const noexcept = 0;
    };

    template <typename T>
    class HandlerInvoker final : public IHandlerInvoker {
    public:
        HandlerInvoker(uint64_t id, std::function<void(const T&)> fn)
            : m_ID(id), m_Fn(std::move(fn)) {}

        [[nodiscard]] uint64_t GetID() const noexcept override { return m_ID; }

        void Invoke(const T& event) const {
            if (m_Fn) {
                m_Fn(event);
            }
        }

    private:
        uint64_t m_ID;
        std::function<void(const T&)> m_Fn;
    };
} // namespace Detail

/// Abonelik kimligi; unregister islemlerinde kullanilir.
struct SubscriptionToken {
    EventTypeID typeID = 0;
    uint64_t handlerID = 0;

    [[nodiscard]] bool IsValid() const noexcept { return handlerID != 0; }
    void Reset() noexcept { typeID = 0; handlerID = 0; }

    bool operator==(const SubscriptionToken& other) const noexcept {
        return typeID == other.typeID && handlerID == other.handlerID;
    }
};

class EventBus;

/// RAII tabanli otomatik abonelik sonlandirici (Scoped Subscription).
/// Kapsamdan cikildiginda veya nesne yok edildiginde dinleyiciyi guvenle siler.
class ScopedSubscription {
public:
    ScopedSubscription() = default;

    ScopedSubscription(EventBus* bus, SubscriptionToken token)
        : m_Bus(bus), m_Token(token) {}

    ~ScopedSubscription() {
        Reset();
    }

    ScopedSubscription(const ScopedSubscription&) = delete;
    ScopedSubscription& operator=(const ScopedSubscription&) = delete;

    ScopedSubscription(ScopedSubscription&& other) noexcept
        : m_Bus(other.m_Bus), m_Token(other.m_Token) {
        other.m_Bus = nullptr;
        other.m_Token.Reset();
    }

    ScopedSubscription& operator=(ScopedSubscription&& other) noexcept {
        if (this != &other) {
            Reset();
            m_Bus = other.m_Bus;
            m_Token = other.m_Token;
            other.m_Bus = nullptr;
            other.m_Token.Reset();
        }
        return *this;
    }

    void Reset();

    [[nodiscard]] bool IsActive() const noexcept {
        return m_Bus != nullptr && m_Token.IsValid();
    }

private:
    EventBus* m_Bus = nullptr;
    SubscriptionToken m_Token{};
};

/// Type-safe, sifir-tahsisli (zero-allocation dispatch) yayin/abone Olay Veriyolu (EventBus).
class EventBus {
public:
    EventBus() = default;
    ~EventBus() = default;

    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;

    EventBus(EventBus&&) noexcept = default;
    EventBus& operator=(EventBus&&) noexcept = default;

    /// Bir olay turune abone olur ve geri alinabilir bir SubscriptionToken dondurur.
    template <typename T>
    SubscriptionToken Subscribe(std::function<void(const T&)> handler) {
        const EventTypeID typeID = Detail::GetEventTypeID<T>();
        const uint64_t handlerID = ++m_NextHandlerID;

        if (typeID >= m_Handlers.size()) {
            m_Handlers.resize(typeID + 1);
        }

        auto invoker = std::make_shared<Detail::HandlerInvoker<T>>(handlerID, std::move(handler));
        m_Handlers[typeID].push_back(std::move(invoker));

        return SubscriptionToken{typeID, handlerID};
    }

    /// Bir olay turune uye fonksiyon ile abone olmayi kolaylastiran yardimci
    template <typename T, typename ClassType>
    SubscriptionToken Subscribe(ClassType* instance, void (ClassType::*memberFunc)(const T&)) {
        return Subscribe<T>([instance, memberFunc](const T& event) {
            (instance->*memberFunc)(event);
        });
    }

    /// Bir olay turune ScopedSubscription olusturarak abone olur.
    template <typename T>
    [[nodiscard]] ScopedSubscription SubscribeScoped(std::function<void(const T&)> handler) {
        SubscriptionToken token = Subscribe<T>(std::move(handler));
        return ScopedSubscription(this, token);
    }

    /// Bir aboneligi sonlandirir.
    void Unsubscribe(SubscriptionToken token) {
        if (!token.IsValid() || token.typeID >= m_Handlers.size()) {
            return;
        }

        auto& list = m_Handlers[token.typeID];
        for (auto it = list.begin(); it != list.end(); ++it) {
            if ((*it)->GetID() == token.handlerID) {
                list.erase(it);
                break;
            }
        }
    }

    /// Olusturulan olayi tum abonelere aninda (senkron) dagitir.
    /// Yayin sirasinda heap tahsisi yapilmaz.
    template <typename T>
    void Publish(const T& event) {
        const EventTypeID typeID = Detail::GetEventTypeID<T>();
        if (typeID >= m_Handlers.size()) {
            return;
        }

        // Calisma sirasinda abone ekleme/cikarmadan etkilenmemek icin mevcut liste kopyalanir
        auto handlersCopy = m_Handlers[typeID];
        for (const auto& invoker : handlersCopy) {
            static_cast<const Detail::HandlerInvoker<T>*>(invoker.get())->Invoke(event);
        }
    }

    /// Tum abonelikleri temizler.
    void Clear() {
        m_Handlers.clear();
    }

    [[nodiscard]] size_t GetRegisteredEventTypeCount() const noexcept {
        return m_Handlers.size();
    }

private:
    uint64_t m_NextHandlerID = 0;
    std::vector<std::vector<std::shared_ptr<Detail::IHandlerInvoker>>> m_Handlers;
};

inline void ScopedSubscription::Reset() {
    if (m_Bus && m_Token.IsValid()) {
        m_Bus->Unsubscribe(m_Token);
        m_Bus = nullptr;
        m_Token.Reset();
    }
}

} // namespace Astral
