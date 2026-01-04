#include "Sdl3Runner.h"
#include "Log/Log.h"

#define SDL_MAIN_HANDLED
#include <SDL3/SDL_main.h>

// Thread-local for passing 'this' to SDL callbacks
namespace { thread_local Sdl::Loop::Sdl3Runner* g_currentSdl3Runner = nullptr; }

namespace Sdl::Loop
{
    Sdl3Runner::Sdl3Runner(Options options)
        : _options{std::move(options)}
        , _updateCtx{*this}
    {
        Log::Trace("created");
    }

    Sdl3Runner::~Sdl3Runner()
    {
        Log::Trace("destroyed");
    }

    void Sdl3Runner::Start(HandlerPtr handler)
    {
        Log::Debug("starting");
        _handler = std::move(handler);
        _updateCtx.Initialize();
        _running = true;

        // Set thread-local for SDL callbacks to access 'this'
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

        g_currentSdl3Runner = nullptr;
        Log::Trace("SDL_EnterAppMainCallbacks returned {}", result);
    }

    void Sdl3Runner::Finish(const App::Loop::FinishData& finishData)
    {
        Log::Debug("finish requested with exit code {}", finishData.ExitCode);
        RequestQuit(finishData.ExitCode);
    }

    void Sdl3Runner::RequestQuit(int exitCode)
    {
        Log::Debug("quit requested with exit code {}", exitCode);
        _exitCode.store(exitCode);
        _running = false;
        
        // Notify any waiters via channel (thread-safe access)
        {
            std::lock_guard lock(_channelMutex);
            if (_quitChannel) {
                _quitChannel->try_send(boost::system::error_code{}, exitCode);
            }
        }
    }

    boost::asio::awaitable<int> Sdl3Runner::WaitForQuit()
    {
        // Already quit? Return immediately
        if (!_running) {
            co_return _exitCode.load();
        }

        // Lazy-create channel using executor from current coroutine
        auto executor = co_await boost::asio::this_coro::executor;
        {
            std::lock_guard lock(_channelMutex);
            if (!_quitChannel) {
                _quitChannel = std::make_shared<QuitChannel>(executor, 1);
            }
        }

        Log::Trace("waiting for quit...");
        auto [ec, exitCode] = co_await _quitChannel->async_receive(
            boost::asio::as_tuple(boost::asio::use_awaitable));
        
        if (ec) {
            Log::Debug("WaitForQuit channel error: {}", ec.message());
        }
        
        co_return exitCode;
    }

    SDL_AppResult SDLCALL Sdl3Runner::AppInit(void** appstate, int /*argc*/, char** /*argv*/)
    {
        auto* self = g_currentSdl3Runner;
        if (!self) {
            Log::Error("AppInit: no runner instance!");
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
        Log::Debug("SDL3 version {}.{}.{}", major, minor, patch);

        // Create window
        Log::Trace("creating window '{}' ({}x{})",
            _options.Window.Title,
            _options.Window.Width,
            _options.Window.Height);

        _window = SDL_CreateWindow(
            _options.Window.Title,
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
        } else {
            Log::Debug("VSync set to {}", _options.VSync);
        }

        if (_options.OnInited) {
            if (! _options.OnInited(*this)) {
                Log::Error("OnInited callback failed");
                return SDL_APP_FAILURE;
            }
        }

        if (!_handler->Started(*this)) {
            Log::Error("Started handler failed");
            return SDL_APP_FAILURE;
        }

        Log::Trace("window and renderer created successfully");
        return SDL_APP_CONTINUE;
    }

    void Sdl3Runner::DoQuit()
    {
        Log::Debug("shutting down...");

        _handler->Stopping(*this);

        if (_options.OnQuitting) {
            _options.OnQuitting(*this);
        }

        if (_renderer) {
            SDL_DestroyRenderer(_renderer);
            _renderer = nullptr;
        }

        if (_window) {
            SDL_DestroyWindow(_window);
            _window = nullptr;
        }

        _running = false;
        _handler.reset();

        Log::Trace("SDL cleanup complete");
    }

    SDL_AppResult Sdl3Runner::DoIterate()
    {
        if (!_running) {
            return SDL_APP_SUCCESS;
        }

        // Update timing
        _updateCtx.Tick();

        // Call update action
        if (!_handler->Update(*this, _updateCtx)) {
            Log::Debug("update handler is stopping");
            return SDL_APP_SUCCESS;
        }

        //TODO: if handler Update did not render anything - Default: clear with dark blue
        // SDL_SetRenderDrawColor(_renderer, 30, 30, 80, 255);
        // SDL_RenderClear(_renderer);

        // Call render callback
        if (_options.OnRender) {
            _options.OnRender(_renderer, _updateCtx);
        } else {
            //TODO: Default: clear with dark blue if handler Update did not render anything
            // SDL_SetRenderDrawColor(_renderer, 30, 30, 80, 255);
            // SDL_RenderClear(_renderer);
        }

        SDL_RenderPresent(_renderer);
        return SDL_APP_CONTINUE;
    }

    SDL_AppResult Sdl3Runner::DoEvent(SDL_Event* event)
    {
        // TODO: remove handing quit events from inner logic, let user handle them
        if (event->type == SDL_EVENT_QUIT) {
            Log::Debug("received SDL_EVENT_QUIT");
            RequestQuit(0);
            return SDL_APP_SUCCESS;
        }

        if (event->type == SDL_EVENT_KEY_DOWN) {
            if (event->key.key == SDLK_ESCAPE) {
                Log::Debug("ESC pressed, quitting");
                RequestQuit(0);
                return SDL_APP_SUCCESS;
            }
        }

        // Forward to user callback
        _handler->Sdl3Event(*this, *event);

        if (_options.OnEvent) {
            _options.OnEvent(*event);
        }

        return SDL_APP_CONTINUE;
    }
}
