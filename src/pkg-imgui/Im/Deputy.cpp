#include "Deputy.h"
#include "Log/Log.h"

#include "Sdl/RendererScopes.h"
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_video.h>

#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"
#include "imgui_internal.h"

#include <filesystem>

namespace Im
{
    static const float DefaultFontSize = 15.0f;
    static const auto DefaultFontsDir = std::filesystem::current_path() / "data" / "fonts";
    static const auto DefaultUiFont = "Roboto-Medium.ttf";
    static const auto DefaultMonoFont = "JetBrainsMono-Bold.ttf";
    static const std::array DefaultFonts = {
        DefaultUiFont,
        DefaultMonoFont,
    };

    static Log::Logger _internalImGuiLogger{"ImGui"};
    Log::Logger Deputy::_logger = Log::Logger("Im.Deputy");

    Deputy::Deputy(SDL_Window* window, SDL_Renderer* renderer)
        : _window(window)
        , _renderer(renderer)
    {
        // context
        IMGUI_CHECKVERSION();
        _logger.Debug("ImGUI {}", ImGui::GetVersion());

        auto* context = ImGui::CreateContext();
        _context = context;

        ImGuiIO& io = ImGui::GetIO();
        _io = &io;

        io.IniFilename = nullptr; // disable saving ini file
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

        // error handling
        _context->ErrorCallback = [](ImGuiContext* ctx, void* user_data, const char* msg) { 
            _internalImGuiLogger.Msg({}, Log::Level::Error, msg);
        };
        io.ConfigErrorRecoveryEnableAssert = false; // disable asserts on errors (don't crash app)

        // scaling
        io.ConfigDpiScaleFonts = true;      // [EXPERIMENTAL] Automatically overwrite style.FontScaleDpi when Monitor DPI changes. This will scale fonts but _NOT_ scale sizes/padding for now.
        io.ConfigDpiScaleViewports = true;  // [EXPERIMENTAL] Scale Dear ImGui and Platform Windows when Monitor DPI changes.

        // style
        ImGui::StyleColorsDark();
        ImGuiStyle& style = ImGui::GetStyle();

        const auto& fbScale = _io->DisplayFramebufferScale;
        _logger.Trace("fb-scale: {}x{}", fbScale.x, fbScale.y);

        auto contentScale = SDL_GetDisplayContentScale(SDL_GetDisplayForWindow(_window));
        _logger.Trace("style: content-scale={}", contentScale);
        style.ScaleAllSizes(contentScale);
        // style.FontScaleDpi = scale; // using io.ConfigDpiScaleFonts=true makes this unnecessary
        _logger.Trace("style: font: SizeBase={} ScaleMain={} ScaleDpi={}", style.FontSizeBase, style.FontScaleMain, style.FontScaleDpi);

        LoadFonts();

        // backend/renderer
        ImGui_ImplSDL3_InitForSDLRenderer(_window, _renderer);
        ImGui_ImplSDLRenderer3_Init(_renderer);
    }

    Deputy::~Deputy()
    {
        ImGui_ImplSDLRenderer3_Shutdown();
        ImGui_ImplSDL3_Shutdown();

        _io = {};
        ImGui::DestroyContext(_context);
        _context = {};
    }

    void Deputy::LoadFonts()
    {
        bool font_loaded = false;
        for (const auto& font_name : DefaultFonts) {
            const auto font_path = DefaultFontsDir / font_name;
            const auto font_path_str = font_path.string();
            if (_io->Fonts->AddFontFromFileTTF(font_path_str.c_str(), DefaultFontSize) != nullptr) {
                _logger.Debug("Font loaded: {}", font_path_str);
                font_loaded = true;
            } else {
                _logger.Warn("Font loading failed: {} -> {}", font_name, font_path_str);
            }
        }
        if (!font_loaded) {
            _logger.Warn("Failed to load any font, using default");
        }
    }

    void Deputy::UpdateBegin()
    {
        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        const auto& fbScale = _io->DisplayFramebufferScale;
        static bool fbScaleLogged = false;
        if (!fbScaleLogged) {
            _logger.Trace("fb-scale: {}x{}", fbScale.x, fbScale.y);
            fbScaleLogged = true;
        }

        // docking
        _dockSpaceId = ImGui::DockSpaceOverViewport(0, nullptr, ImGuiDockNodeFlags_PassthruCentralNode);
    }

    void Deputy::UpdateEnd()
    {
        ImGui::Render();

        auto* drawData = ImGui::GetDrawData();
        Sdl::SetRenderScaleScope scaleScope{
            _renderer,
            _io->DisplayFramebufferScale.x,
            _io->DisplayFramebufferScale.y
        };
        ImGui_ImplSDLRenderer3_RenderDrawData(drawData, _renderer);
    }

    void Deputy::ProcessSdlEvent(const SDL_Event& event)
    {
        ImGui_ImplSDL3_ProcessEvent(&event);
    }
}
