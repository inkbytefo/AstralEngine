#include "TestFramework.hpp"
#include "Astral/Core/Events/EventBus.hpp"
#include "Astral/Core/Events/EngineEvents.hpp"
#include <string>

namespace Astral::Test {

struct PlayerDamagedEvent {
    int damage = 0;
    std::string source;
};

struct ScoreChangedEvent {
    int newScore = 0;
};

class MockListener {
public:
    int damageReceived = 0;
    void OnPlayerDamaged(const PlayerDamagedEvent& e) {
        damageReceived += e.damage;
    }
};

void RunEventBusTests() {
    const std::string suite = "EventBusArchitectureSuite";

    EventBus bus;

    // 1. Temel Yayim ve Dinleme (Publish & Subscribe)
    int damageSum = 0;
    auto token1 = bus.Subscribe<PlayerDamagedEvent>([&damageSum](const PlayerDamagedEvent& e) {
        damageSum += e.damage;
    });
    TEST_CHECK(suite, "TokenValidOnSubscribe", token1.IsValid());

    bus.Publish(PlayerDamagedEvent{25, "Lava"});
    TEST_CHECK(suite, "EventReceivedAndHandled", damageSum == 25);

    // 2. Coklu Dinleyici (Multiple Listeners)
    int listener2Count = 0;
    auto token2 = bus.Subscribe<PlayerDamagedEvent>([&listener2Count](const PlayerDamagedEvent& /*e*/) {
        listener2Count++;
    });
    TEST_CHECK(suite, "Token2Valid", token2.IsValid());

    bus.Publish(PlayerDamagedEvent{10, "Spike"});
    TEST_CHECK(suite, "FirstListenerUpdatedAgain", damageSum == 35);
    TEST_CHECK(suite, "SecondListenerCalled", listener2Count == 1);

    // 3. Bagimsiz Olay Turleri Izolasyonu (Different Event Types Isolation)
    int score = 0;
    bus.Subscribe<ScoreChangedEvent>([&score](const ScoreChangedEvent& e) {
        score = e.newScore;
    });

    bus.Publish(ScoreChangedEvent{500});
    TEST_CHECK(suite, "ScoreEventReceived", score == 500);
    TEST_CHECK(suite, "DamageUntouchedByScoreEvent", damageSum == 35);

    // 4. Uye Fonksiyon Yardimcisi (Member Function Subscription)
    MockListener mockObj;
    auto mockToken = bus.Subscribe<PlayerDamagedEvent>(&mockObj, &MockListener::OnPlayerDamaged);
    bus.Publish(PlayerDamagedEvent{15, "Laser"});
    TEST_CHECK(suite, "MemberFunctionListenerHandled", mockObj.damageReceived == 15);

    // 5. Abonelikten Cikma (Manual Unsubscribe via Token)
    bus.Unsubscribe(token1);
    bus.Unsubscribe(mockToken);
    bus.Publish(PlayerDamagedEvent{5, "Fire"});
    TEST_CHECK(suite, "UnsubscribedListenerNotCalled", damageSum == 50); // 35 + 15 (mock) = 50, but +5 ignored by token1
    TEST_CHECK(suite, "MockListenerUnsubscribed", mockObj.damageReceived == 15);
    TEST_CHECK(suite, "RemainingListenerStillActive", listener2Count == 3);

    // 6. RAII ScopedSubscription Otomatik Sonlandirma
    int scopedCount = 0;
    {
        ScopedSubscription scopedSub = bus.SubscribeScoped<ScoreChangedEvent>([&scopedCount](const ScoreChangedEvent& /*e*/) {
            scopedCount++;
        });
        TEST_CHECK(suite, "ScopedSubIsActive", scopedSub.IsActive());
        bus.Publish(ScoreChangedEvent{1000});
        TEST_CHECK(suite, "ScopedSubReceivedEvent", scopedCount == 1);
    }
    // Scope sonlandi, abonelik sonlandirilmis olmali
    bus.Publish(ScoreChangedEvent{2000});
    TEST_CHECK(suite, "ScopedSubAutomaticallyUnsubscribed", scopedCount == 1);

    // 7. Standart Motor Olaylari (EngineEvents) Dogrulamasi
    bool entitySelectedReceived = false;
    EntityHandle selectedHandle{};
    bus.Subscribe<EntitySelectedEvent>([&](const EntitySelectedEvent& e) {
        entitySelectedReceived = true;
        selectedHandle = e.entity;
    });

    EntityHandle fakeHandle = MakeEntityHandle(42, 1);
    bus.Publish(EntitySelectedEvent{fakeHandle, nullptr});
    TEST_CHECK(suite, "EngineEntitySelectedEventHandled", entitySelectedReceived && selectedHandle == fakeHandle);

    // 8. Temizleme (Clear)
    bus.Clear();
    bus.Publish(PlayerDamagedEvent{100, "Nuke"});
    TEST_CHECK(suite, "ClearRemovesAllHandlers", listener2Count == 3);
}

} // namespace Astral::Test
