#include "Sdl3Looper.h"
#include "Log/Log.h"

#define SDL_MAIN_HANDLED
#include <SDL3/SDL_main.h>

// Thread-local for passing 'this' to SDL callbacks
namespace { thread_local Sdl::Loop::Sdl3Looper* g_currentSdl3Looper = nullptr; }

namespace Sdl::Loop
{
    Sdl3Looper::Sdl3Looper(Options options)
        : _options{std::move(options)}
        , _updateCtx{*this}
    {
        Log::Trace("created");
    }

    Sdl3Looper::~Sdl3Looper()
    {
        Log::Trace("destroyed");
    }

    bool Sdl3Looper::Initialize()
    {
        Log::Debug("initializing SDL3...");

        // Log SDL version
        int version = SDL_GetVersion();
        int major = SDL_VERSIONNUM_MAJOR(version);
        int minor = SDL_VERSIONNUM_MINOR(version);
        int patch = SDL_VERSIONNUM_MICRO(version);
        Log::Info("SDL version {}.{}.{}", major, minor, patch);

        // Create window
        Log::Debug("creating window '{}' ({}x{})",
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
            return false;
        }

        // Create renderer
        _renderer = SDL_CreateRenderer(_window, nullptr);
        if (!_renderer) {
            Log::Error("SDL_CreateRenderer failed: {}", SDL_GetError());
            SDL_DestroyWindow(_window);
            _window = nullptr;
            return false;
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
                return false;
            }
        }

        Log::Trace("window and renderer created successfully");
        return true;
    }

    void Sdl3Looper::Shutdown()
    {
        Log::Debug("shutting down...");

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

        Log::Trace("SDL cleanup complete");
    }

    void Sdl3Looper::Start(UpdateAction updateAction)
    {
        Log::Debug("starting");
        _updateAction = std::move(updateAction);
        _updateCtx.Initialize();
        _running = true;

        // Set thread-local for SDL callbacks to access 'this'
        g_currentSdl3Looper = this;

        // Use SDL's cross-platform main loop implementation
        // This handles platform-specific details (macOS app delegate, emscripten, etc.)
        int result = SDL_EnterAppMainCallbacks(
            0, nullptr,  // argc/argv not needed, we already have them in Domain
            AppInit,
            AppIterate,
            AppEvent,
            AppQuit
        );

        g_currentSdl3Looper = nullptr;
        Log::Trace("SDL_EnterAppMainCallbacks returned {}", result);
    }

    // SDL App Callbacks - called by SDL's cross-platform loop
    SDL_AppResult SDLCALL Sdl3Looper::AppInit(void** appstate, int /*argc*/, char** /*argv*/)
    {
        auto* self = g_currentSdl3Looper;
        
        if (!self) {
            Log::Error("AppInit: no looper instance!");
            return SDL_APP_FAILURE;
        }
        
        *appstate = self;
        
        if (!self->Initialize()) {
            return SDL_APP_FAILURE;
        }
        
        return SDL_APP_CONTINUE;
    }

    SDL_AppResult SDLCALL Sdl3Looper::AppIterate(void* appstate)
    {
        auto* self = static_cast<Sdl3Looper*>(appstate);
        
        if (!self->_running) {
            return SDL_APP_SUCCESS;
        }

        // Update timing
        self->_updateCtx.Tick();

        // Call update action (for io_context.poll())
        if (self->_updateAction && !self->_updateAction(self->_updateCtx)) {
            Log::Debug("update action returned false, stopping");
            return SDL_APP_SUCCESS;
        }

        // Render
        self->DoRender();

        return self->_running ? SDL_APP_CONTINUE : SDL_APP_SUCCESS;
    }

    SDL_AppResult SDLCALL Sdl3Looper::AppEvent(void* appstate, SDL_Event* event)
    {
        auto* self = static_cast<Sdl3Looper*>(appstate);

        // Handle quit events
        if (event->type == SDL_EVENT_QUIT) {
            Log::Debug("received SDL_EVENT_QUIT");
            self->RequestQuit(0);
            return SDL_APP_SUCCESS;
        }

        if (event->type == SDL_EVENT_KEY_DOWN) {
            if (event->key.key == SDLK_ESCAPE) {
                Log::Debug("ESC pressed, quitting");
                self->RequestQuit(0);
                return SDL_APP_SUCCESS;
            }
        }

        // Forward to user callback
        if (self->_options.OnEvent) {
            self->_options.OnEvent(*event);
        }

        return SDL_APP_CONTINUE;
    }

    void SDLCALL Sdl3Looper::AppQuit(void* appstate, SDL_AppResult result)
    {
        Log::Trace("result={}", static_cast<int>(result)); //TODO: enum traits to string
        auto* self = static_cast<Sdl3Looper*>(appstate);
        self->Shutdown();
    }

    void Sdl3Looper::DoRender()
    {
        if (_options.OnRender) {
            _options.OnRender(_renderer, _updateCtx);
        } else {
            // Default: clear with dark blue
            SDL_SetRenderDrawColor(_renderer, 30, 30, 80, 255);
            SDL_RenderClear(_renderer);
        }

        SDL_RenderPresent(_renderer);
    }

    void Sdl3Looper::Finish(const FinishData& finishData)
    {
        Log::Debug("finish requested with exit code {}", finishData.ExitCode);
        RequestQuit(finishData.ExitCode);
    }

    void Sdl3Looper::RequestQuit(int exitCode)
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

    boost::asio::awaitable<int> Sdl3Looper::WaitForQuit()
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
}
