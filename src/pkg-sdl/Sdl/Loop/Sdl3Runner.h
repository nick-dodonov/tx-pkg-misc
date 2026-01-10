#pragma once
#include "App/Loop/Handler.h"
#include "App/Loop/Runner.h"

#include <SDL3/SDL.h>

#include <atomic>
#include <boost/asio.hpp>
#include <boost/asio/experimental/channel.hpp>
#include <functional>
#include <mutex>

namespace Sdl::Loop
{
    /// SDL3 runner handler extending the base handler (Started/Stopping/Update)
    class Sdl3Handler : public App::Loop::Handler<class Sdl3Runner>
    {
    public:
        using Sdl3Runner = class Sdl3Runner;

        /// SDL3-specific event callback
        virtual SDL_AppResult Sdl3Event(Sdl3Runner& runner, const SDL_Event& event) { return SDL_APP_CONTINUE; }
    };

    /// SDL3-based runner that uses SDL_EnterAppMainCallbacks for cross-platform support
    class Sdl3Runner final : public App::Loop::Runner<Sdl3Handler>
    {
    public:
        struct WindowConfig
        {
            const char* Title = "SDL3 App";
            int Width = 800;
            int Height = 600;
            SDL_WindowFlags Flags = SDL_WINDOW_RESIZABLE;
        };

        struct Options
        {
            WindowConfig Window{};
            SDL_InitFlags InitFlags = SDL_INIT_VIDEO;

            /// VSync setting (1 = enabled, 0 = disabled, -1 = adaptive)
            /// Enabled by default
            int VSync = 1;

            /// Optional initialized callback, called once after SDL initialization
            std::function<bool(Sdl3Runner& runner)> OnInited;

            /// Optional quitting callback, called once before shutdown
            std::function<void(Sdl3Runner& runner)> OnQuitting;

            /// Optional render callback, called each frame with renderer and timing context
            std::function<void(SDL_Renderer* renderer, const App::Loop::UpdateCtx& ctx)> OnRender;

            /// Optional event callback, called for each SDL event
            std::function<SDL_AppResult(const SDL_Event&)> OnEvent;
        };

        explicit Sdl3Runner(HandlerPtr handler, Options options);
        ~Sdl3Runner() override;

        // IRunner interface
        void Start() override;
        void Finish(const App::Loop::FinishData& finishData) override;

        // Sdl3-specific accessors
        [[nodiscard]] SDL_Window* GetWindow() const { return _window; }
        [[nodiscard]] SDL_Renderer* GetRenderer() const { return _renderer; }
        [[nodiscard]] bool IsRunning() const { return _running; }

        /// Awaitable that completes when quit is requested
        /// Returns the exit code
        boost::asio::awaitable<int> WaitQuit();

    private:
        Options _options;

        SDL_Window* _window = nullptr;
        SDL_Renderer* _renderer = nullptr;

        App::Loop::UpdateCtx _updateCtx;
        std::atomic<bool> _running{false};
        std::atomic<int> _exitCode{0};

        // For async quit notification (created lazily in WaitForQuit)
        using QuitChannel = boost::asio::experimental::channel<void(boost::system::error_code, int)>;
        std::mutex _channelMutex;
        std::shared_ptr<QuitChannel> _quitChannel;

        void SignalQuit();

        // SDL App callbacks (static, called by SDL)
        static SDL_AppResult SDLCALL AppInit(void** appstate, int argc, char** argv);
        static void SDLCALL AppQuit(void* appstate, SDL_AppResult result);
        static SDL_AppResult SDLCALL AppIterate(void* appstate);
        static SDL_AppResult SDLCALL AppEvent(void* appstate, SDL_Event* event);

        // Internal helpers
        SDL_AppResult DoInit();
        void DoQuit();
        SDL_AppResult DoIterate();
        SDL_AppResult DoEvent(SDL_Event* event);
    };
}
