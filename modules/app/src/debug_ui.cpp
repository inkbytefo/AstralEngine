#include "debug_ui.hpp"

#include <astral/platform/window.hpp>
#include <astral/render/text.hpp>

#include <imgui.h>

#include <algorithm>
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
    case Key::Delete: return ImGuiKey_Delete;
    case Key::Insert: return ImGuiKey_Insert;
    case Key::Home: return ImGuiKey_Home;
    case Key::End: return ImGuiKey_End;
    case Key::PageUp: return ImGuiKey_PageUp;
    case Key::PageDown: return ImGuiKey_PageDown;
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
    case Key::LeftSuper: return ImGuiKey_LeftSuper;
    case Key::RightSuper: return ImGuiKey_RightSuper;
    case Key::Minus: return ImGuiKey_Minus;
    case Key::Equal: return ImGuiKey_Equal;
    case Key::LeftBracket: return ImGuiKey_LeftBracket;
    case Key::RightBracket: return ImGuiKey_RightBracket;
    case Key::Backslash: return ImGuiKey_Backslash;
    case Key::Semicolon: return ImGuiKey_Semicolon;
    case Key::Apostrophe: return ImGuiKey_Apostrophe;
    case Key::Grave: return ImGuiKey_GraveAccent;
    case Key::Comma: return ImGuiKey_Comma;
    case Key::Period: return ImGuiKey_Period;
    case Key::Slash: return ImGuiKey_Slash;
    default: return ImGuiKey_None;
    }
}

CursorShape to_cursor(ImGuiMouseCursor c) {
    switch (c) {
    case ImGuiMouseCursor_TextInput: return CursorShape::TextInput;
    case ImGuiMouseCursor_ResizeAll: return CursorShape::ResizeAll;
    case ImGuiMouseCursor_ResizeNS: return CursorShape::ResizeNS;
    case ImGuiMouseCursor_ResizeEW: return CursorShape::ResizeEW;
    case ImGuiMouseCursor_ResizeNESW: return CursorShape::ResizeNESW;
    case ImGuiMouseCursor_ResizeNWSE: return CursorShape::ResizeNWSE;
    case ImGuiMouseCursor_Hand: return CursorShape::Hand;
    case ImGuiMouseCursor_NotAllowed: return CursorShape::NotAllowed;
    default: return CursorShape::Arrow;
    }
}

ImTextureID pack(TextureHandle h) { return static_cast<ImTextureID>(pack_ui_texture(h)); }
TextureHandle unpack(ImTextureID id) { return unpack_ui_texture(static_cast<std::uint64_t>(id)); }

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

DebugUi::DebugUi(Renderer& renderer, Window* window, std::string_view ini_path)
    : renderer_(renderer), window_(window), ini_path_(ini_path) {
    IMGUI_CHECKVERSION();
    context_ = ImGui::CreateContext();
    ImGui::SetCurrentContext(context_);
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = ini_path_.empty() ? nullptr : ini_path_.c_str();
    io.BackendRendererName = "astral";
    io.BackendPlatformName = "astral_sdl3";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures | ImGuiBackendFlags_RendererHasVtxOffset |
                       ImGuiBackendFlags_HasMouseCursors;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
#ifdef IMGUI_HAS_DOCK
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
#endif
    ImGuiPlatformIO& pio = ImGui::GetPlatformIO();
    pio.Platform_ClipboardUserData = this;
    pio.Platform_GetClipboardTextFn = [](ImGuiContext*) -> const char* {
        auto* self = static_cast<DebugUi*>(ImGui::GetPlatformIO().Platform_ClipboardUserData);
        if (!self || !self->window_) return "";
        self->clipboard_cache_ = self->window_->clipboard_text();
        return self->clipboard_cache_.c_str();
    };
    pio.Platform_SetClipboardTextFn = [](ImGuiContext*, const char* text) {
        auto* self = static_cast<DebugUi*>(ImGui::GetPlatformIO().Platform_ClipboardUserData);
        if (self && self->window_ && text) self->window_->set_clipboard_text(text);
    };

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
    const float scale = std::max(density, 1.0f);
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(static_cast<float>(size.width) / scale, static_cast<float>(size.height) / scale);
    io.DisplayFramebufferScale = ImVec2(scale, scale);
    io.DeltaTime = delta > 0.0f ? delta : 1.0f / 60.0f;
    ImGui::GetStyle().FontScaleDpi = scale;

    const Vec2 mouse = input.mouse_position();
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
    io.AddKeyEvent(ImGuiMod_Super, input.down(Key::LeftSuper) || input.down(Key::RightSuper));
    if (!input.text().empty()) io.AddInputCharactersUTF8(std::string(input.text()).c_str());
    ImGui::NewFrame();
}

void DebugUi::update_textures() {
    for (ImTextureData* t : ImGui::GetPlatformIO().Textures) {
        if (t->Status == ImTextureStatus_WantCreate) {
            t->SetTexID(pack(renderer_.create_texture(to_image(*t))));
            t->SetStatus(ImTextureStatus_OK);
        } else if (t->Status == ImTextureStatus_WantUpdates) {
            renderer_.update_texture(unpack(t->GetTexID()), to_image(*t));
            t->SetStatus(ImTextureStatus_OK);
        } else if (t->Status == ImTextureStatus_WantDestroy && t->UnusedFrames > 4) {
            renderer_.destroy(unpack(t->GetTexID()));
            t->SetTexID(ImTextureID_Invalid);
            t->SetStatus(ImTextureStatus_Destroyed);
        }
    }
}

void DebugUi::update_cursor() {
    if (!window_) return;
    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange) return;
    window_->set_cursor(to_cursor(ImGui::GetMouseCursor()));
}

void DebugUi::end() {
    ImGui::SetCurrentContext(context_);
    update_cursor();
    ImGui::Render();
    update_textures();
    vertices_.clear();
    indices_.clear();
    commands_.clear();
    const ImDrawData* data = ImGui::GetDrawData();
    if (!data) return;
    const float sx = data->FramebufferScale.x;
    const float sy = data->FramebufferScale.y;
    for (const ImDrawList* list : data->CmdLists) {
        const auto vertex_base = static_cast<std::int32_t>(vertices_.size());
        const auto index_base = static_cast<std::uint32_t>(indices_.size());
        for (const ImDrawVert& v : list->VtxBuffer) {
            vertices_.push_back({{(v.pos.x - data->DisplayPos.x) * sx, (v.pos.y - data->DisplayPos.y) * sy},
                                 {v.uv.x, v.uv.y}, v.col});
        }
        for (ImDrawIdx i : list->IdxBuffer) indices_.push_back(i);
        for (const ImDrawCmd& c : list->CmdBuffer) {
            if (c.UserCallback || c.ElemCount == 0) continue;
            commands_.push_back({.first_index = index_base + c.IdxOffset, .index_count = c.ElemCount,
                                 .vertex_offset = vertex_base + static_cast<std::int32_t>(c.VtxOffset),
                                 .texture = unpack(c.GetTexID()),
                                 .clip = {(c.ClipRect.x - data->DisplayPos.x) * sx, (c.ClipRect.y - data->DisplayPos.y) * sy,
                                          (c.ClipRect.z - data->DisplayPos.x) * sx, (c.ClipRect.w - data->DisplayPos.y) * sy},
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
