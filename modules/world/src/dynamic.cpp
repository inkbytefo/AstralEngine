// Script-defined components and by-name events.
#include <astral/world/event_registry.hpp>
#include <astral/world/serialize.hpp>

#include <array>
#include <memory>
#include <utility>

namespace astral {

namespace {

constexpr std::size_t max_words = 64; // 256 bytes per script-defined component

/// Plain data of N 32-bit words: trivially relocatable, so one set of operations per size.
template <std::size_t N> struct Blob {
    alignas(4) std::byte bytes[N * 4];
};

template <std::size_t N> ComponentInfo blob_info(ComponentId id) {
    ComponentInfo info = component_info<Blob<N>>();
    info.id = id;
    return info;
}

using BlobMaker = ComponentInfo (*)(ComponentId);

template <std::size_t... I> constexpr std::array<BlobMaker, sizeof...(I)> blob_makers(std::index_sequence<I...>) {
    return {&blob_info<I + 1>...};
}

/// What a script-defined component owns; shared with the world so the layout outlives every row.
struct DynamicType {
    char tag = 0; // its address is the ComponentId
    ComponentInfo info{};
    std::vector<FieldInfo> fields;
    std::vector<Json> defaults;
    std::size_t words = 1;

    void fill_defaults(void* base, const LoadContext& ctx) const {
        std::memset(base, 0, words * 4);
        for (std::size_t i = 0; i < fields.size(); ++i) {
            if (!defaults[i].is_null()) field_from_json(fields[i], base, defaults[i], ctx);
        }
    }
};

std::uint32_t kind_size(FieldKind k) {
    switch (k) {
    case FieldKind::Bool: return 4;
    case FieldKind::Int:
    case FieldKind::UInt:
    case FieldKind::Float: return 4;
    case FieldKind::Vec2: return 8;
    case FieldKind::Vec3: return 12;
    case FieldKind::Vec4:
    case FieldKind::Quat: return 16;
    case FieldKind::EntityRef: return 8;
    default: return 0;
    }
}

} // namespace

const TypeRegistry::Entry* TypeRegistry::add_dynamic(std::string_view name, std::vector<FieldDef> defs, std::string* error) {
    const auto fail = [&](std::string message) -> const Entry* {
        if (error) *error = std::string(name) + ": " + std::move(message);
        return nullptr;
    };
    if (name.empty()) return fail("a component needs a name");
    if (find(name)) return fail("a component with this name already exists");

    auto type = std::make_shared<DynamicType>();
    std::uint32_t offset = 0;
    for (FieldDef& d : defs) {
        const std::uint32_t size = kind_size(d.kind);
        if (size == 0) return fail("field \"" + d.name + "\" has a type that is not plain data (" + std::string(field_kind_name(d.kind)) + ")");
        if (d.name.empty() || find_field(type->fields, d.name)) return fail("field names must be unique and not empty");
        FieldInfo f;
        f.name = d.name;
        f.kind = d.kind;
        f.offset = offset;
        f.size = d.kind == FieldKind::Bool ? 1 : (d.kind == FieldKind::Int || d.kind == FieldKind::UInt || d.kind == FieldKind::Float) ? 4 : size;
        f.is_signed = d.kind != FieldKind::UInt;
        type->fields.push_back(std::move(f));
        type->defaults.push_back(std::move(d.default_value));
        offset += size;
    }
    type->words = std::max<std::size_t>(1, offset / 4);
    if (type->words > max_words) return fail("too large (" + std::to_string(offset) + " bytes; the limit is 256)");
    static const auto makers = blob_makers(std::make_index_sequence<max_words>{});
    type->info = makers[type->words - 1](reinterpret_cast<ComponentId>(&type->tag));

    owned_names_.emplace_back(name);
    const std::string_view stable_name = owned_names_.back();
    const auto make_value = [type](const Json& j, const LoadContext& in, std::array<std::byte, max_words * 4>& buffer) {
        type->fill_defaults(buffer.data(), in);
        if (j.is_object()) {
            for (const FieldInfo& f : type->fields) {
                const Json* member = j.find(f.name);
                if (!member) continue;
                LoadContext inner = in;
                inner.path = in.path + "." + f.name;
                field_from_json(f, buffer.data(), *member, inner);
            }
        } else if (!j.is_null()) {
            in.error("expected an object");
        }
    };
    const auto save_value = [type](const std::byte* base, const SaveContext& ctx) {
        Json o = Json::object();
        for (const FieldInfo& f : type->fields) o[f.name] = field_to_json(f, base, ctx);
        return o;
    };

    entries_.push_back(Entry{
        stable_name, stable_hash(stable_name), type->info.id,
        [type](World& w, Entity e) { return w.has_raw(e, type->info.id); },
        [type, save_value](World& w, Entity e, const SaveContext& ctx) {
            const std::byte* base = static_cast<const World&>(w).get_raw(e, type->info.id);
            return base ? save_value(base, ctx) : Json(nullptr);
        },
        [type, make_value](World& w, Entity e, const Json& j, const LoadContext& ctx) {
            std::array<std::byte, max_words * 4> buffer{};
            make_value(j, ctx, buffer);
            w.keep_alive(type);
            w.add_raw(e, type->info, buffer.data());
        },
        [type](World& w, Entity e) { w.remove_raw(e, type->info.id); },
        [type, make_value, save_value](const Json& j, const LoadContext& in, const SaveContext& out) {
            std::array<std::byte, max_words * 4> buffer{};
            make_value(j, in, buffer);
            return save_value(buffer.data(), out);
        },
        type->fields, true});
    return &entries_.back();
}

void EventRegistry::add_dynamic(std::string_view name) {
    if (find(name)) return;
    owned_names_.emplace_back(name);
    const std::string_view stable = owned_names_.back();
    entries_.push_back(Entry{
        stable,
        [stable](World& w, const Json& payload, std::vector<std::string>*) { w.send(DynamicEvent{std::string(stable), payload}); },
        [stable](World& w, std::uint64_t& cursor, const std::function<void(const Json&)>& fn) {
            const Events<DynamicEvent>* events = w.resource<Events<DynamicEvent>>();
            if (!events) return std::size_t{0};
            std::size_t n = 0;
            events->read(cursor, [&](const DynamicEvent& e) {
                if (e.name != stable) return;
                fn(e.payload);
                ++n;
            });
            return n;
        },
        {}, true});
}

const EventRegistry::Entry* EventRegistry::find(std::string_view name) const noexcept {
    for (const Entry& e : entries_) {
        if (e.name == name) return &e;
    }
    return nullptr;
}

} // namespace astral
