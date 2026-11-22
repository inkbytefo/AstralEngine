#include "debug_ui.hpp"

#include <astral/render/text.hpp>
#include <astral/world/scene.hpp>

#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace astral {

namespace {

ImGuiKey to_imgui(Key k) {
    const auto i = static_cast<int>(k);
    if (k >= Key::A && k <= Key::Z) return static_cast<ImGuiKey>(ImGuiKey_A + (i - static_cast<int>(Key::A)));
    if (k >= Key::Num0 && k <= Key::Num9) return static_cast<ImGuiKey>(ImGuiKey_0 + (i - static_cast<int>(Key::Num0)));
    if (k >= Key::F1 && k <= Key::F12) return static_cast<ImGuiKey>(ImGuiKey_F1 + (i - static_cast<int>(Key::F1)));
    switch (k) {
    case Key::Space: return ImGuiKey_Space;
    case Key::Enter: return ImGuiKey_Enter;
    case Key::Escape: return ImGuiKey_Escape;
    case Key::Tab: return ImGuiKey_Tab;
    case Key::Backspace: return ImGuiKey_Backspace;
    case Key::Left: return ImGuiKey_LeftArrow;
    case Key::Right: return ImGuiKey_RightArrow;
    case Key::Up: return ImGuiKey_UpArrow;
    case Key::Down: return ImGuiKey_DownArrow;
    case Key::LeftShift: return ImGuiKey_LeftShift;
    case Key::RightShift: return ImGuiKey_RightShift;
    case Key::LeftCtrl: return ImGuiKey_LeftCtrl;
    case Key::RightCtrl: return ImGuiKey_RightCtrl;
    case Key::LeftAlt: return ImGuiKey_LeftAlt;
    case Key::RightAlt: return ImGuiKey_RightAlt;
    default: return ImGuiKey_None;
    }
}

ImTextureID pack(TextureHandle h) { return (static_cast<ImTextureID>(h.generation) << 32) | h.index; }
TextureHandle unpack(ImTextureID id) {
    return {static_cast<std::uint32_t>(id & 0xFFFFFFFFu), static_cast<std::uint32_t>(id >> 32)};
}

ImageData to_image(ImTextureData& t) {
    ImageData img{.width = static_cast<std::uint32_t>(t.Width), .height = static_cast<std::uint32_t>(t.Height), .rgba = {},
                  .srgb = false, .format = ImageFormat::RGBA8, .levels = {}};
    const auto* px = static_cast<const std::uint8_t*>(t.GetPixels());
    const std::size_t n = std::size_t{img.width} * img.height;
    if (t.Format == ImTextureFormat_Alpha8) {
        img.rgba.resize(n * 4);
        for (std::size_t i = 0; i < n; ++i) {
            img.rgba[i * 4 + 0] = img.rgba[i * 4 + 1] = img.rgba[i * 4 + 2] = 255;
            img.rgba[i * 4 + 3] = px[i];
        }
    } else {
        img.rgba.assign(px, px + n * 4);
    }
    return img;
}

// --- JSON editor: every reflected component and setting becomes editable ----------------------

bool contains(std::string_view text, std::string_view part) { return text.find(part) != std::string_view::npos; }

bool edit_json(const char* label, Json& j, int depth = 0) {
    bool changed = false;
    ImGui::PushID(label);
    switch (j.type()) {
    case Json::Type::Null: ImGui::TextDisabled("%s: none", label); break;
    case Json::Type::Bool: {
        bool b = j.as_bool();
        if (ImGui::Checkbox(label, &b)) {
            j = b;
            changed = true;
        }
        break;
    }
    case Json::Type::Number: {
        float f = static_cast<float>(j.as_number());
        if (ImGui::DragFloat(label, &f, 0.01f, 0.0f, 0.0f, "%.3f")) {
            j = static_cast<double>(f);
            changed = true;
        }
        break;
    }
    case Json::Type::String: {
        char buffer[256];
        std::snprintf(buffer, sizeof buffer, "%s", j.as_string().c_str());
        if (ImGui::InputText(label, buffer, sizeof buffer, ImGuiInputTextFlags_EnterReturnsTrue)) {
            j = std::string(buffer);
            changed = true;
        }
        break;
    }
    case Json::Type::Array: {
        const auto& a = j.as_array();
        const bool numbers = a.size() >= 2 && a.size() <= 4 &&
                             std::all_of(a.begin(), a.end(), [](const Json& v) { return v.is_number(); });
        if (numbers) {
            float v[4] = {};
            for (std::size_t i = 0; i < a.size(); ++i) v[i] = static_cast<float>(a[i].as_number());
            bool edited = false;
            const bool colour = contains(label, "color") || contains(label, "colour") || contains(label, "emissive") ||
                                contains(label, "sky") || contains(label, "ground");
            if (colour && a.size() == 3) edited = ImGui::ColorEdit3(label, v, ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float);
            else if (colour && a.size() == 4) edited = ImGui::ColorEdit4(label, v, ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float);
            else if (a.size() == 2) edited = ImGui::DragFloat2(label, v, 0.01f);
            else if (a.size() == 3) edited = ImGui::DragFloat3(label, v, 0.01f);
            else edited = ImGui::DragFloat4(label, v, 0.01f);
            if (edited) {
                Json out = Json::array();
                for (std::size_t i = 0; i < a.size(); ++i) out.push_back(static_cast<double>(v[i]));
                j = std::move(out);
                changed = true;
            }
        } else if (ImGui::TreeNode(label, "%s [%zu]", label, a.size())) {
            Json copy = j;
            for (std::size_t i = 0; i < copy.size(); ++i) {
                Json item = copy[i];
                const std::string name = "[" + std::to_string(i) + "]";
                if (edit_json(name.c_str(), item, depth + 1)) {
                    Json out = Json::array();
                    for (std::size_t k = 0; k < copy.size(); ++k) out.push_back(k == i ? item : copy[k]);
                    j = std::move(out);
                    changed = true;
                }
            }
            ImGui::TreePop();
        }
        break;
    }
    case Json::Type::Object: {
        const bool open = depth == 0 || ImGui::TreeNode(label);
        if (open) {
            const std::vector<std::pair<std::string, Json>> members(j.as_object().begin(), j.as_object().end());
            for (const auto& [key, value] : members) {
                Json v = value;
                if (edit_json(key.c_str(), v, depth + 1)) {
                    j[key] = std::move(v);
                    changed = true;
                }
            }
            if (depth > 0) ImGui::TreePop();
        }
        break;
    }
    }
    ImGui::PopID();
    return changed;
}

std::string entity_label(World& world, Entity e) {
    std::string label;
    if (const Name* n = world.get<const Name>(e); n && !n->value.empty()) label = n->value;
    else label = "Entity " + std::to_string(e.index);
    if (world.has<PrefabInstance>(e)) label += "  [prefab]";
    return label;
}

} // namespace

DebugUi::DebugUi(Renderer& renderer) : renderer_(renderer) {
    IMGUI_CHECKVERSION();
    context_ = ImGui::CreateContext();
    ImGui::SetCurrentContext(context_);
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr; // no imgui.ini next to the game
    io.BackendRendererName = "astral";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures | ImGuiBackendFlags_RendererHasVtxOffset;
    const auto font = default_font_ttf();
    ImFontConfig config;
    config.FontDataOwnedByAtlas = false; // embedded in the binary
    io.Fonts->AddFontFromMemoryTTF(const_cast<std::uint8_t*>(font.data()), static_cast<int>(font.size()), 15.0f, &config);
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 4.0f;
    style.FrameRounding = 3.0f;
    style.Colors[ImGuiCol_WindowBg].w = 0.88f;
}

DebugUi::~DebugUi() {
    ImGui::SetCurrentContext(context_);
    for (ImTextureData* t : ImGui::GetPlatformIO().Textures) {
        if (t->GetTexID() != ImTextureID_Invalid) renderer_.destroy(unpack(t->GetTexID()));
    }
    ImGui::DestroyContext(context_);
}

void DebugUi::begin(const Input& input, float delta, gpu::Extent2D size, float density) {
    ImGui::SetCurrentContext(context_);
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(static_cast<float>(size.width), static_cast<float>(size.height));
    io.DeltaTime = delta > 0.0f ? delta : 1.0f / 60.0f;
    ImGui::GetStyle().FontScaleDpi = density;

    const Vec2 mouse = input.mouse_position() * density;
    io.AddMousePosEvent(mouse.x, mouse.y);
    for (int b = 0; b < 3; ++b) {
        const bool down = input.down(static_cast<MouseButton>(b));
        if (down != mouse_down_[b]) io.AddMouseButtonEvent(b, down);
        mouse_down_[b] = down;
    }
    if (input.scroll().x != 0.0f || input.scroll().y != 0.0f) io.AddMouseWheelEvent(input.scroll().x, input.scroll().y);
    for (int k = 1; k < static_cast<int>(Key::Count) && k < 256; ++k) {
        const bool down = input.down(static_cast<Key>(k));
        if (down == keys_down_[k]) continue;
        keys_down_[k] = down;
        if (const ImGuiKey key = to_imgui(static_cast<Key>(k)); key != ImGuiKey_None) io.AddKeyEvent(key, down);
    }
    io.AddKeyEvent(ImGuiMod_Ctrl, input.down(Key::LeftCtrl) || input.down(Key::RightCtrl));
    io.AddKeyEvent(ImGuiMod_Shift, input.down(Key::LeftShift) || input.down(Key::RightShift));
    io.AddKeyEvent(ImGuiMod_Alt, input.down(Key::LeftAlt) || input.down(Key::RightAlt));
    if (!input.text().empty()) io.AddInputCharactersUTF8(std::string(input.text()).c_str());
    ImGui::NewFrame();
}

void DebugUi::update_textures() {
    for (ImTextureData* t : ImGui::GetPlatformIO().Textures) {
        if (t->Status == ImTextureStatus_WantCreate) {
            t->SetTexID(pack(renderer_.create_texture(to_image(*t))));
            t->SetStatus(ImTextureStatus_OK);
        } else if (t->Status == ImTextureStatus_WantUpdates) {
            // Whole re-upload: font atlases grow rarely; the handle stays the same.
            renderer_.update_texture(unpack(t->GetTexID()), to_image(*t));
            t->SetStatus(ImTextureStatus_OK);
        } else if (t->Status == ImTextureStatus_WantDestroy && t->UnusedFrames > 4) {
            renderer_.destroy(unpack(t->GetTexID()));
            t->SetTexID(ImTextureID_Invalid);
            t->SetStatus(ImTextureStatus_Destroyed);
        }
    }
}

void DebugUi::end() {
    ImGui::SetCurrentContext(context_);
    ImGui::Render();
    update_textures();
    vertices_.clear();
    indices_.clear();
    commands_.clear();
    const ImDrawData* data = ImGui::GetDrawData();
    if (!data) return;
    for (const ImDrawList* list : data->CmdLists) {
        const auto vertex_base = static_cast<std::int32_t>(vertices_.size());
        const auto index_base = static_cast<std::uint32_t>(indices_.size());
        for (const ImDrawVert& v : list->VtxBuffer) vertices_.push_back({{v.pos.x, v.pos.y}, {v.uv.x, v.uv.y}, v.col});
        for (ImDrawIdx i : list->IdxBuffer) indices_.push_back(i);
        for (const ImDrawCmd& c : list->CmdBuffer) {
            if (c.UserCallback || c.ElemCount == 0) continue;
            commands_.push_back({.first_index = index_base + c.IdxOffset, .index_count = c.ElemCount,
                                 .vertex_offset = vertex_base + static_cast<std::int32_t>(c.VtxOffset),
                                 .texture = unpack(c.GetTexID()),
                                 .clip = {c.ClipRect.x - data->DisplayPos.x, c.ClipRect.y - data->DisplayPos.y,
                                          c.ClipRect.z - data->DisplayPos.x, c.ClipRect.w - data->DisplayPos.y},
                                 .sdf_softness = 0.0f});
        }
    }
}

UiLayer DebugUi::layer() const { return {.vertices = vertices_, .indices = indices_, .commands = commands_}; }

bool DebugUi::wants_mouse() const { return ImGui::GetCurrentContext() == context_ && ImGui::GetIO().WantCaptureMouse; }
bool DebugUi::wants_keyboard() const { return ImGui::GetCurrentContext() == context_ && ImGui::GetIO().WantCaptureKeyboard; }
bool DebugUi::wants_text() const { return ImGui::GetCurrentContext() == context_ && ImGui::GetIO().WantTextInput; }

void DebugUi::draw_panels(Context& ctx) {
    if (ImGui::BeginMainMenuBar()) {
        ImGui::TextDisabled("Astral");
        if (ImGui::BeginMenu("Windows")) {
            ImGui::MenuItem("Performance", nullptr, &show_performance_);
            ImGui::MenuItem("Scene", nullptr, &show_scene_);
            ImGui::MenuItem("Inspector", nullptr, &show_inspector_);
            ImGui::MenuItem("Render", nullptr, &show_render_);
            ImGui::MenuItem("World", nullptr, &show_world_);
            ImGui::MenuItem("ImGui demo", nullptr, &show_demo_);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Debug draw")) {
            ImGui::MenuItem("Physics shapes", nullptr, &ctx.debug_show.physics);
            ImGui::MenuItem("Lights", nullptr, &ctx.debug_show.lights);
            ImGui::MenuItem("World cells", nullptr, &ctx.debug_show.world_cells);
            ImGui::EndMenu();
        }
        ImGui::SameLine(ImGui::GetWindowWidth() - 200);
        ImGui::TextDisabled("F1 hides | %.1f ms", static_cast<double>(ctx.profiler->last_frame_ms()));
        ImGui::EndMainMenuBar();
    }

    if (show_world_) {
        ImGui::SetNextWindowSize(ImVec2(300, 260), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("World", &show_world_)) {
            const WorldPosition o = ctx.world_origin();
            ImGui::Text("Origin: %.1f, %.1f, %.1f", o.x, o.y, o.z);
            if (const StreamingStats* st = ctx.world_streaming()) {
                ImGui::Text("Cells: %u in world, %u loading, %u waiting", st->attached, st->loading, st->waiting);
                ImGui::Text("Last frame: +%u / -%u", st->attached_last_update, st->detached_last_update);
                ImGui::Checkbox("Draw cells", &ctx.debug_show.world_cells);
                if (MapLayers* layers = ctx.world_layers(); layers && !layers->layers().empty()) {
                    ImGui::SeparatorText("Data layers");
                    for (const LayerDesc& l : layers->layers()) {
                        bool on = l.active;
                        if (ImGui::Checkbox(l.name.c_str(), &on)) layers->set(l.name, on);
                    }
                }
            } else {
                ImGui::TextDisabled("No map open (ctx.open_world).");
            }
        }
        ImGui::End();
    }

    if (show_performance_) {
        ImGui::SetNextWindowPos(ImVec2(10, 32), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(360, 420), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Performance", &show_performance_)) {
            const std::vector<float> times = ctx.profiler->frame_times();
            float average = 0, worst = 0;
            for (float t : times) {
                average += t;
                worst = std::max(worst, t);
            }
            average = times.empty() ? 0.0f : average / static_cast<float>(times.size());
            ImGui::Text("%.1f FPS  |  %.2f ms avg  |  %.2f ms worst", average > 0 ? 1000.0 / average : 0.0,
                        static_cast<double>(average), static_cast<double>(worst));
            if (!times.empty()) {
                ImGui::PlotLines("##frames", times.data(), static_cast<int>(times.size()), 0, nullptr, 0.0f,
                                 std::max(33.3f, worst), ImVec2(-1, 60));
            }
            if (ImGui::CollapsingHeader("CPU", ImGuiTreeNodeFlags_DefaultOpen) &&
                ImGui::BeginTable("cpu", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
                for (const ProfileSample& s : ctx.profiler->last_frame()) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    // Indent(0) would mean "the default indent": only indent nested scopes.
                    const float indent = static_cast<float>(s.depth) * 14.0f;
                    if (indent > 0.0f) ImGui::Indent(indent);
                    ImGui::TextUnformatted(s.name);
                    if (indent > 0.0f) ImGui::Unindent(indent);
                    ImGui::TableNextColumn();
                    ImGui::Text("%.3f ms", static_cast<double>(s.ms));
                }
                ImGui::EndTable();
            }
            if (ctx.renderer && ImGui::CollapsingHeader("GPU", ImGuiTreeNodeFlags_DefaultOpen)) {
                const auto timings = ctx.renderer->gpu_timings();
                if (timings.empty()) ImGui::TextDisabled("no timestamps yet");
                else ImGui::Text("total %.3f ms", static_cast<double>(ctx.renderer->gpu_frame_ms()));
                if (!timings.empty() && ImGui::BeginTable("gpu", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
                    for (const PassTiming& t : timings) {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(t.name.c_str());
                        ImGui::TableNextColumn();
                        ImGui::Text("%.3f ms", static_cast<double>(t.ms));
                    }
                    ImGui::EndTable();
                }
                const RenderStats& st = ctx.renderer->stats();
                ImGui::TextDisabled("%s | %u draws, %u instances, %u lights, %u SDF", st.ray_traced ? "ray traced" : "raster",
                                    st.draw_calls, st.instances, st.point_lights + st.rect_lights, st.sdf_shapes);
            }
        }
        ImGui::End();
    }

    World& world = ctx.world;
    if (selected_.valid() && !world.alive(selected_)) selected_ = {};
    if (show_scene_) {
        ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 330, 32), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(320, 300), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Scene", &show_scene_)) {
            char buffer[128];
            std::snprintf(buffer, sizeof buffer, "%s", filter_.c_str());
            if (ImGui::InputTextWithHint("##filter", "filter by name", buffer, sizeof buffer)) filter_ = buffer;
            std::vector<Entity> shown;
            world.for_each_entity([&](Entity e) {
                if (world.has<PrefabPart>(e)) return; // parts are listed under their instance in the inspector
                if (!filter_.empty() && entity_label(world, e).find(filter_) == std::string::npos) return;
                shown.push_back(e);
            });
            ImGui::TextDisabled("%zu entities", shown.size());
            if (ImGui::BeginChild("list")) {
                ImGuiListClipper clipper;
                clipper.Begin(static_cast<int>(shown.size()));
                while (clipper.Step()) {
                    for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                        const Entity e = shown[static_cast<std::size_t>(i)];
                        ImGui::PushID(static_cast<int>(e.index));
                        if (ImGui::Selectable(entity_label(world, e).c_str(), e == selected_)) selected_ = e;
                        ImGui::PopID();
                    }
                }
            }
            ImGui::EndChild();
        }
        ImGui::End();
    }

    if (show_inspector_) {
        ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 330, 340), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(320, 380), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Inspector", &show_inspector_)) {
            if (!selected_.valid()) {
                ImGui::TextDisabled("select an entity in Scene");
            } else {
                ImGui::Text("%s", entity_label(world, selected_).c_str());
                ImGui::TextDisabled("index %u, generation %u", selected_.index, selected_.generation);
                if (const PrefabInstance* pi = world.get<const PrefabInstance>(selected_)) ImGui::TextDisabled("prefab %s", pi->path.c_str());
                std::vector<Entity> parts;
                world.each<const PrefabPart>([&](Entity p, const PrefabPart& part) {
                    if (part.instance == selected_) parts.push_back(p);
                });
                if (!parts.empty() && ImGui::TreeNode("parts", "%zu prefab parts", parts.size())) {
                    for (Entity p : parts) {
                        ImGui::PushID(static_cast<int>(p.index));
                        if (ImGui::Selectable(entity_label(world, p).c_str())) selected_ = p;
                        ImGui::PopID();
                    }
                    ImGui::TreePop();
                }
                const Entity e = selected_;
                const TypeRegistry::Entry* remove = nullptr;
                for (const TypeRegistry::Entry& t : ctx.types.entries()) {
                    if (!t.has(world, e)) continue;
                    const std::string name(t.name);
                    ImGui::PushID(name.c_str());
                    const bool open = ImGui::CollapsingHeader(name.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap);
                    ImGui::SameLine(ImGui::GetWindowWidth() - 30);
                    if (ImGui::SmallButton("x")) remove = &t;
                    if (open) {
                        Json value = component_to_json(world, t, e);
                        if (edit_json(name.c_str(), value)) component_from_json(world, t, e, value);
                    }
                    ImGui::PopID();
                }
                if (remove) remove->remove(world, e);
                if (ImGui::BeginCombo("##add", "Add component")) {
                    for (const TypeRegistry::Entry& t : ctx.types.entries()) {
                        if (t.has(world, e)) continue;
                        if (ImGui::Selectable(std::string(t.name).c_str())) component_from_json(world, t, e, Json::object());
                    }
                    ImGui::EndCombo();
                }
                if (ImGui::Button("Destroy entity")) {
                    world.destroy(e);
                    selected_ = {};
                }
            }
        }
        ImGui::End();
    }

    if (show_render_) {
        ImGui::SetNextWindowPos(ImVec2(10, 460), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(360, 300), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Render", &show_render_)) {
            if (ctx.renderer) {
                Json settings = to_json(ctx.renderer->settings(), {});
                if (ImGui::CollapsingHeader("Settings", ImGuiTreeNodeFlags_DefaultOpen) && edit_json("settings", settings)) {
                    RenderSettings s = ctx.renderer->settings();
                    from_json(settings, s, {});
                    ctx.renderer->set_settings(s);
                }
                if (ImGui::Button("Reset history")) ctx.renderer->reset_history();
            }
            Json sun = to_json(ctx.sun, {});
            if (ImGui::CollapsingHeader("Sun") && edit_json("sun", sun)) from_json(sun, ctx.sun, {});
            Json env = to_json(ctx.environment, {});
            if (ImGui::CollapsingHeader("Environment") && edit_json("environment", env)) from_json(env, ctx.environment, {});
        }
        ImGui::End();
    }
    if (show_demo_) ImGui::ShowDemoWindow(&show_demo_);
}

} // namespace astral
