#pragma once

#include <SDL3/SDL.h>

#include "App/Loop/ILooper.h"
#include <boost/asio.hpp>
#include <boost/asio/experimental/channel.hpp>
#include <functional>
#include <atomic>
#include <mutex>

namespace Sdl::Loop
{
    // Import types from App::Loop
    using App::Loop::ILooper;
    using App::Loop::UpdateCtx;
    using App::Loop::FinishData;

    /// SDL3-based looper that uses SDL_EnterAppMainCallbacks for cross-platform support
    class Sdl3Looper final : public ILooper
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

            /// Optional render callback, called each frame with renderer and timing context
            std::function<void(SDL_Renderer*, const UpdateCtx&)> OnRender;

            /// Optional event callback, called for each SDL event
            std::function<void(const SDL_Event&)> OnEvent;
        };

        explicit Sdl3Looper(Options options);
        ~Sdl3Looper() override;

        // ILooper interface
        void Start(UpdateAction updateAction) override;
        void Finish(const FinishData& finishData) override;

        // SDL3-specific accessors
        [[nodiscard]] SDL_Window* GetWindow() const { return _window; }
        [[nodiscard]] SDL_Renderer* GetRenderer() const { return _renderer; }
        [[nodiscard]] bool IsRunning() const { return _running; }

        /// Awaitable that completes when quit is requested
        /// Returns the exit code
        boost::asio::awaitable<int> WaitForQuit();

        /// Request quit from external code
        void RequestQuit(int exitCode = 0);

    private:
        Options _options;
        SDL_Window* _window = nullptr;
        SDL_Renderer* _renderer = nullptr;

        UpdateAction _updateAction;
        UpdateCtx _updateCtx;
        std::atomic<bool> _running{false};
        std::atomic<int> _exitCode{0};

        // For async quit notification (created lazily in WaitForQuit)
        using QuitChannel = boost::asio::experimental::channel<void(boost::system::error_code, int)>;
        std::mutex _channelMutex;
        std::shared_ptr<QuitChannel> _quitChannel;

        // SDL App callbacks (static, called by SDL)
        static SDL_AppResult SDLCALL AppInit(void** appstate, int argc, char* argv[]);
        static SDL_AppResult SDLCALL AppIterate(void* appstate);
        static SDL_AppResult SDLCALL AppEvent(void* appstate, SDL_Event* event);
        static void SDLCALL AppQuit(void* appstate, SDL_AppResult result);

        // Internal helpers
        bool Initialize();
        void Shutdown();
        void DoRender();
    };
}
