#include "Sdl3Runner.h"
#include "Log/Log.h"

#define SDL_MAIN_HANDLED
#include <SDL3/SDL_main.h>
#if __EMSCRIPTEN__
#include <emscripten.h>
#endif

// Thread-local for passing 'this' to SDL callbacks
namespace { thread_local Sdl::Loop::Sdl3Runner* g_currentSdl3Runner = nullptr; }

namespace Sdl::Loop
{
    Sdl3Runner::Sdl3Runner(HandlerPtr handler, Sdl3HandlerPtr sdlHandler, Options options)
        : Runner{std::move(handler)}
        , _sdlHandler{std::move(sdlHandler)}
        , _options{std::move(options)}
        , _updateCtx{*this}
    {
        Log::Trace("created");
    }

    Sdl3Runner::~Sdl3Runner()
    {
        Log::Trace("destroy");
    }

    void Sdl3Runner::Start()
    {
        Log::Debug("...");
        _updateCtx.Initialize();
        _running = true;

        // Pass instance to setup appstate in AppInit
        g_currentSdl3Runner = this;

        // Use SDL's cross-platform main loop implementation
        // This handles platform-specific details (macOS app delegate, emscripten, etc.)
        int result = SDL_EnterAppMainCallbacks(
            0, nullptr,  // argc/argv not needed, we already have them in Domain
            AppInit,
            AppIterate,
            AppEvent,
            AppQuit
        );
        Log::Trace("SDL_EnterAppMainCallbacks returned {}", result);

#if __EMSCRIPTEN__
        // SDL_EnterAppMainCallbacks exits immediately in Emscripten
        //  but we need to wait for quit signal, so complete current execution flow
        Log::Debug("emscripten_exit_with_live_runtime...");
        emscripten_exit_with_live_runtime();
#endif
    }

    void Sdl3Runner::Finish(const App::Loop::FinishData& finishData)
    {
        Log::Debug("requested: {}", finishData.ExitCode);
        _exitCode.store(finishData.ExitCode);
        SignalQuit();
    }

    boost::asio::awaitable<int> Sdl3Runner::WaitQuit()
    {
        Log::Trace("waiting for quit...");

        // Already quit? Return immediately
        if (!_running) {
            auto exitCode = _exitCode.load();
            Log::Trace("complete immediately: {}", exitCode);
            co_return exitCode;
        }

        // Lazy-create channel using executor from current coroutine
        auto executor = co_await boost::asio::this_coro::executor;
        {
            std::lock_guard lock(_channelMutex);
            if (!_quitChannel) {
                _quitChannel = std::make_shared<QuitChannel>(executor, 1);
            }
        }

        auto [ec, exitCode] = co_await _quitChannel->async_receive(
            boost::asio::as_tuple(boost::asio::use_awaitable));
        Log::Trace("complete on signal: {} ({})", exitCode, ec.message());

        co_return exitCode;
    }

    void Sdl3Runner::SignalQuit()
    {
        Log::Trace("exitting...");
        auto exitCode = _exitCode.load();
        _running = false;

        // Notify any waiters via channel (thread-safe access)
        {
            std::lock_guard lock(_channelMutex);
            if (_quitChannel) {
                _quitChannel->try_send(boost::system::error_code{}, exitCode);
            }
        }

#if __EMSCRIPTEN__
        // pospone runtime exit 
        // - allow application to handle quit event and cleanup
        // - to avoid issues with calling from inside SDL main loop
        // - allow runtime exit after inside SDL_AppQuit
        // NOLINTBEGIN reinterpret_cast
        emscripten_async_call(
            [](void* state) {
                auto exitCode = reinterpret_cast<int>(state);
                Log::Trace("emscripten_force_exit: {}", exitCode);
                emscripten_force_exit(exitCode);
            },
            reinterpret_cast<void*>(exitCode),
            0
        );
        // NOLINTEND
#endif
    }

    SDL_AppResult SDLCALL Sdl3Runner::AppInit(void** appstate, int /*argc*/, char** /*argv*/)
    {
        auto* self = g_currentSdl3Runner;
        g_currentSdl3Runner = nullptr;

        if (!self) {
            Log::Fatal("internal error: no runner instance");
            return SDL_APP_FAILURE;
        }

        *appstate = self;
        return self->DoInit();
    }

    void SDLCALL Sdl3Runner::AppQuit(void* appstate, SDL_AppResult result)
    {
        Log::Trace("result={}", static_cast<int>(result)); //TODO: enum traits to string
        auto* self = static_cast<Sdl3Runner*>(appstate);
        self->DoQuit();
    }

    SDL_AppResult SDLCALL Sdl3Runner::AppIterate(void* appstate)
    {
        auto* self = static_cast<Sdl3Runner*>(appstate);
        return self->DoIterate();
    }

    SDL_AppResult SDLCALL Sdl3Runner::AppEvent(void* appstate, SDL_Event* event)
    {
        auto* self = static_cast<Sdl3Runner*>(appstate);
        return self->DoEvent(event);
    }

    SDL_AppResult Sdl3Runner::DoInit()
    {
        int version = SDL_GetVersion();
        int major = SDL_VERSIONNUM_MAJOR(version);
        int minor = SDL_VERSIONNUM_MINOR(version);
        int patch = SDL_VERSIONNUM_MICRO(version);
        Log::Debug("SDL3 {}.{}.{} '{}' {}x{} vsync={}", 
            major, minor, patch,
            _options.Window.Title,
            _options.Window.Width,
            _options.Window.Height,
            _options.VSync
        );

        // Create window
        _window = SDL_CreateWindow(
            _options.Window.Title.c_str(),
            _options.Window.Width,
            _options.Window.Height,
            _options.Window.Flags
        );

        if (!_window) {
            Log::Error("SDL_CreateWindow failed: {}", SDL_GetError());
            return SDL_APP_FAILURE;
        }

        // Create renderer
        _renderer = SDL_CreateRenderer(_window, nullptr);
        if (!_renderer) {
            Log::Error("SDL_CreateRenderer failed: {}", SDL_GetError());
            SDL_DestroyWindow(_window);
            _window = nullptr;
            return SDL_APP_FAILURE;
        }

        // Set VSync
        if (!SDL_SetRenderVSync(_renderer, _options.VSync)) {
            Log::Warn("SDL_SetRenderVSync({}) not supported, using disabled", _options.VSync);
            SDL_SetRenderVSync(_renderer, SDL_RENDERER_VSYNC_DISABLED);
        }

        if (!InvokeStarted()) {
            Log::Error("Started handler failed");
            return SDL_APP_FAILURE;
        }

        Log::Trace("completed");
        return SDL_APP_CONTINUE;
    }

    void Sdl3Runner::DoQuit()
    {
        Log::Debug("shutting down...");
        SignalQuit();

        InvokeStopping();

        if (_renderer) {
            SDL_DestroyRenderer(_renderer);
            _renderer = nullptr;
        }

        if (_window) {
            SDL_DestroyWindow(_window);
            _window = nullptr;
        }

        _running = false;

        Log::Trace("shutdown complete");
    }

    SDL_AppResult Sdl3Runner::DoIterate()
    {
        if (!_running) {
            return SDL_APP_SUCCESS;
        }

        // Update timing
        _updateCtx.Tick();

        // Call update action
        if (!InvokeUpdate(_updateCtx)) {
            Log::Debug("update handler is stopping");
            return SDL_APP_SUCCESS;
        }

        //TODO: if handler Update did not render anything - Default: clear with dark blue
        // SDL_SetRenderDrawColor(_renderer, 30, 30, 80, 255);
        // SDL_RenderClear(_renderer);

        SDL_RenderPresent(_renderer);
        return SDL_APP_CONTINUE;
    }

    SDL_AppResult Sdl3Runner::DoEvent(SDL_Event* event)
    {
        // Forward to user callback
        auto rc = _sdlHandler->Sdl3Event(*this, *event);
        if (rc != SDL_APP_CONTINUE) {
            return rc;
        }

        return SDL_APP_CONTINUE;
    }
}
