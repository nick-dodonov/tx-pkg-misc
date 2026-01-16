#pragma once
#include "App/Loop/Handler.h"
#include "App/Loop/Runner.h"
#include "Sdl/Sdl3Ptr.h"
#include <atomic>

namespace Sdl::Loop
{
    class Sdl3Runner;

    /// SDL3 runner handler w/ event except base (Start/Stop/Update)
    class Sdl3Handler
    {
    public:
        virtual ~Sdl3Handler() = default;

        /// SDL3-specific event callback
        virtual SDL_AppResult Sdl3Event(Sdl3Runner& runner, const SDL_Event& event) { return SDL_APP_CONTINUE; }
    };

    /// SDL3-based runner that uses SDL events for cross-platform support
    class Sdl3Runner final : public App::Loop::Runner
    {
    public:
        struct WindowConfig
        {
            std::string Title = "SDL3 App";
            int Width = 800;
            int Height = 600;
            SDL_WindowFlags Flags = 
                SDL_WINDOW_RESIZABLE
                //| SDL_WINDOW_HIDDEN
                | SDL_WINDOW_HIGH_PIXEL_DENSITY
                | SDL_WINDOW_FILL_DOCUMENT
                ;
        };

        struct Options
        {
            WindowConfig Window{};
            SDL_InitFlags InitFlags = SDL_INIT_VIDEO;

            /// VSync setting (1 = enabled, 0 = disabled, -1 = adaptive)
            /// Enabled by default
            int VSync = 1;
        };

        using Sdl3HandlerPtr = std::shared_ptr<Sdl3Handler>;

        explicit Sdl3Runner(HandlerPtr handler, Sdl3HandlerPtr sdlHandler, Options options);
        ~Sdl3Runner() override;

        // IRunner interface
        int Run() override;
        void Exit(int exitCode) override;

        // Sdl3-specific accessors
        [[nodiscard]] SDL_Window* GetWindow() const { return _window.get(); }
        [[nodiscard]] SDL_Renderer* GetRenderer() const { return _renderer.get(); }
        [[nodiscard]] bool IsRunning() const { return _running; }

    private:
        Sdl3HandlerPtr _sdlHandler;
        Options _options;

        // Keep-alive for emscripten async callbacks
        std::shared_ptr<Sdl3Runner> _selfRef;

        Window _window;
        Renderer _renderer;

        App::Loop::UpdateCtx _updateCtx;
        std::atomic<bool> _running{false};

        static SDL_AppResult SDLCALL AppInit(void** appstate, int argc, char** argv);

        // Internal helpers
        SDL_AppResult DoInit();
        void DoQuit(SDL_AppResult result);
        SDL_AppResult DoIterate();
        SDL_AppResult DoEvent(SDL_Event* event);
    };
}
