#include "Deputy.h"
#include "Log/Log.h"

#include "Fs/System.h"
#include "Fs/Drive.h"
#include "Fs/OverlayDrive.h"
#include "Fs/RunfilesDrive.h"

#include "Sdl/RendererScopes.h"
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_video.h>

#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"
#include "imgui_internal.h"

namespace Im
{
    static const float DefaultFontSize = 15.0f;
    static const auto DefaultFontsDir = Fs::Path("data/fonts");
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
        _context->ErrorCallback = [](ImGuiContext* ctx, void* user_data, const char* msg) { _internalImGuiLogger.Msg({}, Log::Level::Error, msg); };
        io.ConfigErrorRecoveryEnableAssert = false; // disable asserts on errors (don't crash app)

        // scaling
        io.ConfigDpiScaleFonts = true; // [EXPERIMENTAL] Automatically overwrite style.FontScaleDpi when Monitor DPI changes. This will scale fonts but _NOT_
                                       // scale sizes/padding for now.
        io.ConfigDpiScaleViewports = true; // [EXPERIMENTAL] Scale Dear ImGui and Platform Windows when Monitor DPI changes.

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

    //TODO: share in Fs and get rid of statics
    static Fs::Drive* GetModuleDataDrive()
    {
        constexpr auto moduleName = "tx-pkg-misc";
        static Fs::Drive* _drive;
        if (_drive) {
            return _drive;
        }
        _drive = &Fs::System::GetDefaultDrive();
        static Fs::RunfilesDrive runfilesDrive(moduleName, _drive);
        if (runfilesDrive.IsSupported()) {
            static Fs::OverlayDrive overlayDrive({&runfilesDrive, _drive});
            Log::Debug("Runfiles drive for module '{}'", moduleName);
            _drive = &overlayDrive;
        } else {
            Log::Debug("Default drive");
        }
        return _drive;
    }

    void Deputy::LoadFonts()
    {
        auto* drive = GetModuleDataDrive();

        bool font_loaded = false;
        for (const auto& font_name : DefaultFonts) {
            auto font_path = DefaultFontsDir / font_name;

            //TODO: speedup and share
            {
                auto result = drive->GetNativePath(font_path);
                if (!result.has_value()) {
#if !__ANDROID__
                    _logger.Warn("Resolve failed: {} (error: {})", font_path.c_str(), result.error().message());
                    continue;
#endif
                } else {
                    auto& native_path = result.value();
                    _logger.Trace("Resolved path: {} -> {}", font_path.c_str(), native_path.c_str());
                    font_path = std::move(native_path);
                }
            }

            if (AddFontFromFileTTF(font_path, DefaultFontSize)) {
                _logger.Debug("Loaded: {}", font_name);
                font_loaded = true;
            } else {
                _logger.Warn("Loading failed: {}", font_path.c_str());
            }
        }

        if (!font_loaded) {
            _logger.Warn("Failed to load any font, using default");
        }
    }

    bool Deputy::AddFontFromFileTTF(const Fs::Path& path, float size_pixels)
    {
#if __ANDROID__
        // On Android default is using the "assets" storage for font files, which is read-only and doesn't support fopen() (required by ImGui's default font loading implementation).
        // To work around this, we are SDL load file abstraction

        // if path starts with "/" remove it - prepare for SDL_LoadFile
        auto* pathStr = path.c_str();
        if (*pathStr == '/') {
            ++pathStr;
        }

        size_t size = 0;
        void* data = SDL_LoadFile(pathStr, &size);
        if (data) {
            ImFontConfig font_cfg = ImFontConfig();
            _io->Fonts->AddFontFromMemoryTTF(data, (int)size, size_pixels);
            ImFormatString(font_cfg.Name, IM_ARRAYSIZE(font_cfg.Name), "%s", path.filename().c_str());
            return true;
        }
        Log::Warn("SDL_LoadFile failed: {}", SDL_GetError());
        return false;
#else
        return _io->Fonts->AddFontFromFileTTF(path.c_str(), size_pixels) != nullptr;
#endif
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

        // https://github.com/ocornut/imgui/wiki/Docking
        _dockSpaceId = ImGui::DockSpaceOverViewport(0, nullptr, ImGuiDockNodeFlags_PassthruCentralNode);
    }

    void Deputy::UpdateEnd()
    {
        ImGui::Render();

        auto* drawData = ImGui::GetDrawData();
        Sdl::SetRenderScaleScope scaleScope{_renderer, _io->DisplayFramebufferScale.x, _io->DisplayFramebufferScale.y};
        ImGui_ImplSDLRenderer3_RenderDrawData(drawData, _renderer);
    }

    void Deputy::ProcessSdlEvent(const SDL_Event& event)
    {
        ImGui_ImplSDL3_ProcessEvent(&event);
    }
}
