#pragma once
#include "RunLoop/Handler.h"
#include "RunLoop/Runner.h"
#include "Sdl/Sdl3Ptr.h"
#include <atomic>

namespace Sdl::Loop
{
    class Sdl3Runner;

    /// SDL3 runner handler w/ event except base (Start/Stop/Update).
    /// Receives a typed Sdl3Runner pointer via injection (set by the runner
    /// constructor/destructor), so derived classes can access SDL resources
    /// without casting.
    class Sdl3Handler
    {
        friend class Sdl3Runner;
        Sdl3Runner* _sdl3Runner{};
        void SetSdl3Runner(Sdl3Runner* runner) { _sdl3Runner = runner; }

    public:
        virtual ~Sdl3Handler() = default;

        /// SDL3-specific event callback
        virtual SDL_AppResult Sdl3Event(Sdl3Runner& runner, const SDL_Event& event) { return SDL_APP_CONTINUE; }

    protected:
        /// Typed accessor for the owning Sdl3Runner (available after construction)
        [[nodiscard]] Sdl3Runner& GetSdl3Runner() { return *_sdl3Runner; }
        [[nodiscard]] const Sdl3Runner& GetSdl3Runner() const { return *_sdl3Runner; }

        /// Convenience accessors forwarded from the owning Sdl3Runner
        [[nodiscard]] SDL_Window* GetWindow() const;
        [[nodiscard]] SDL_Renderer* GetRenderer() const;
    };

    /// SDL3-based runner that uses SDL events for cross-platform support
    class Sdl3Runner final : public RunLoop::Runner
    {
    public:
        struct WindowConfig
        {
            std::string Title = "SDL3 App";
            int Width = 800;
            int Height = 600;

            SDL_WindowFlags Flags = 
                0
                | SDL_WINDOW_RESIZABLE
                | SDL_WINDOW_HIGH_PIXEL_DENSITY  // Enable Retina/HiDPI support
                //| SDL_WINDOW_FILL_DOCUMENT    // Fill-document mode (Emscripten only)
                ;
        };

        struct Options
        {
            SDL_InitFlags InitFlags = 
                SDL_INIT_VIDEO
                //| SDL_INIT_GAMEPAD
                ;
            WindowConfig Window{};

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

        RunLoop::UpdateCtx _updateCtx;
        std::atomic<bool> _running{false};

        static SDL_AppResult SDLCALL AppInit(void** appstate, int argc, char** argv);

        // Internal helpers
        SDL_AppResult DoInit();
        void DoQuit(SDL_AppResult result);
        SDL_AppResult DoIterate();
        SDL_AppResult DoEvent(SDL_Event* event);
    };

    // Inline definitions for Sdl3Handler convenience accessors (require complete Sdl3Runner type)
    inline SDL_Window* Sdl3Handler::GetWindow() const { return _sdl3Runner->GetWindow(); }
    inline SDL_Renderer* Sdl3Handler::GetRenderer() const { return _sdl3Runner->GetRenderer(); }
}
