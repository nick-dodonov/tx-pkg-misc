#include "Deputy.h"
#include "Log/Log.h"

#include "SDL3/SDL_render.h"
#include "SDL3/SDL_video.h"

#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"

#include <filesystem>

namespace Im
{
    static const float DefaultFontSize = 15.0f;
    static const auto DefaultFontPath = std::filesystem::current_path() / "data" / "fonts" / "Roboto-Medium.ttf";

    Log::Logger Deputy::_logger = Log::Logger("Im.Deputy");

    Deputy::Deputy(SDL_Window* window, SDL_Renderer* renderer)
        : _window(window)
        , _renderer(renderer)
    {
        // context
        IMGUI_CHECKVERSION();
        _logger.Debug("ImGUI {}", ImGui::GetVersion());

        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        _imGuiIO = &io;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

        // scaling
        io.ConfigDpiScaleFonts = true;      // [EXPERIMENTAL] Automatically overwrite style.FontScaleDpi when Monitor DPI changes. This will scale fonts but _NOT_ scale sizes/padding for now.
        io.ConfigDpiScaleViewports = true;  // [EXPERIMENTAL] Scale Dear ImGui and Platform Windows when Monitor DPI changes.

        // style
        ImGui::StyleColorsDark();
        ImGuiStyle& style = ImGui::GetStyle();

        auto scale = SDL_GetDisplayContentScale(SDL_GetDisplayForWindow(_window));
        _logger.Trace("style: scale: all={}", scale);
        style.ScaleAllSizes(scale);
        // style.FontScaleDpi = scale; // using io.ConfigDpiScaleFonts=true makes this unnecessary
        _logger.Trace("style: font: SizeBase={} ScaleMain={} ScaleDpi={}", style.FontSizeBase, style.FontScaleMain, style.FontScaleDpi);

        // font
        auto size_pixels = DefaultFontSize;
        const auto font_path_str = DefaultFontPath.string();
        _logger.Debug("Loading font: {}", font_path_str);
        io.Fonts->AddFontFromFileTTF(font_path_str.c_str(), size_pixels);

        // backend/renderer
        ImGui_ImplSDL3_InitForSDLRenderer(_window, _renderer);
        ImGui_ImplSDLRenderer3_Init(_renderer);
    }

    Deputy::~Deputy()
    {
        ImGui_ImplSDLRenderer3_Shutdown();
        ImGui_ImplSDL3_Shutdown();

        _imGuiIO = nullptr;
        ImGui::DestroyContext();
    }

    void Deputy::UpdateBegin()
    {
        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        // docking
        ImGui::DockSpaceOverViewport(0, nullptr, ImGuiDockNodeFlags_PassthruCentralNode);
    }

    void Deputy::UpdateEnd()
    {
        ImGui::Render();
        SDL_SetRenderScale(_renderer, _imGuiIO->DisplayFramebufferScale.x, _imGuiIO->DisplayFramebufferScale.y);
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), _renderer);
    }

    void Deputy::ProcessSdlEvent(const SDL_Event& event)
    {
        ImGui_ImplSDL3_ProcessEvent(&event);
    }
}
