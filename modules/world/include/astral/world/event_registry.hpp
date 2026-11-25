#pragma once
// Events by name, for scripts and tools: send an event from JSON, read events as JSON. Typed C++
// events (World::send<T>, EventReader<T>) and script-defined ones travel through the same queues.
//
//     EventRegistry events;
//     events.add<Scored>();                    // reflected C++ event
//     events.add_dynamic("game::DoorOpened");  // a script's event: payload is any JSON
//     events.find("pong::Scored")->send(world, Json::parse(R"({"by": "Player"})").value());

#include <astral/world/fields.hpp>
#include <astral/world/serialize.hpp>

#include <deque>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace astral {

/// The queue behind add_dynamic events.
struct DynamicEvent {
    std::string name;
    Json payload;
};

class EventRegistry {
public:
    struct Entry {
        std::string_view name;
        /// Sends an event built from `payload` (missing fields keep defaults). Problems go to `errors`.
        std::function<void(World&, const Json& payload, std::vector<std::string>* errors)> send;
        /// Calls fn(payload) for events not yet read through `cursor` (start it at 0).
        std::function<std::size_t(World&, std::uint64_t& cursor, const std::function<void(const Json&)>& fn)> read;
        std::vector<FieldInfo> fields; ///< Empty for dynamic events.
        bool dynamic = false;
    };

    /// T must be ASTRAL_REFLECT'ed. Registering again is a no-op.
    template <class T> void add() {
        static_assert(is_reflected_v<T>, "ASTRAL_REFLECT the event before registering it");
        if (find(Reflect<T>::name)) return;
        entries_.push_back(Entry{
            Reflect<T>::name,
            [](World& w, const Json& payload, std::vector<std::string>* errors) {
                T value{};
                from_json(payload, value, live_load_context(w, errors, std::string(Reflect<T>::name)));
                w.send(std::move(value));
            },
            [](World& w, std::uint64_t& cursor, const std::function<void(const Json&)>& fn) {
                const Events<T>* events = w.resource<Events<T>>();
                if (!events) return std::size_t{0};
                const SaveContext ctx = live_save_context(w);
                return events->read(cursor, [&](const T& e) { fn(to_json(e, ctx)); });
            },
            describe_fields<T>(), false});
    }

    /// An event only scripts know. Registering again is a no-op.
    void add_dynamic(std::string_view name);
    /// Undoes the events added since entries().size() was `entry_count` (a failed script load, ADR-0044).
    void rollback_dynamic(std::size_t entry_count);

    [[nodiscard]] const Entry* find(std::string_view name) const noexcept;
    [[nodiscard]] std::span<const Entry> entries() const noexcept { return entries_; }

private:
    std::vector<Entry> entries_;
    std::deque<std::string> owned_names_;
};

} // namespace astral
