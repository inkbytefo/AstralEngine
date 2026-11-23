#include "debug_ui.hpp"

#include <astral/render/text.hpp>

#include <imgui.h>

#include <cstdint>
#include <string>

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

void draw_main_menu_bar(Context& ctx, DebugPanelRegistry& panels) {
    if (!ImGui::BeginMainMenuBar()) return;
    ImGui::TextDisabled("Astral");
    if (ImGui::BeginMenu("Windows")) {
        for (DebugPanelRegistry::Entry& entry : panels.entries()) {
            ImGui::MenuItem(entry.name.c_str(), nullptr, &entry.open);
        }
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

} // namespace

DebugUi::DebugUi(Renderer& renderer) : renderer_(renderer) {
    IMGUI_CHECKVERSION();
    context_ = ImGui::CreateContext();
    ImGui::SetCurrentContext(context_);
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr; // no imgui.ini next to the game
    io.BackendRendererName = "astral";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures | ImGuiBackendFlags_RendererHasVtxOffset;
#ifdef ImGuiConfigFlags_DockingEnable
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
#endif
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

void DebugUi::draw_panels(Context& ctx, DebugPanelRegistry& panels, MaterialLibrary* materials) {
    panel_state_.materials = materials;
    if (panel_state_.selected.valid() && !ctx.world.alive(panel_state_.selected)) panel_state_.selected = {};
    draw_main_menu_bar(ctx, panels);
    panels.draw_open(ctx, panel_state_);
}

} // namespace astral
