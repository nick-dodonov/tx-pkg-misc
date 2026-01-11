#include "Sdl3Runner.h"
#include "Log/Log.h"
#include <boost/describe.hpp>

#define SDL_MAIN_HANDLED
#include <SDL3/SDL_main.h>
#if __EMSCRIPTEN__
#include <emscripten.h>
#endif

BOOST_DESCRIBE_ENUM(SDL_AppResult, SDL_APP_CONTINUE, SDL_APP_SUCCESS, SDL_APP_FAILURE)

namespace
{
    // Thread-local for passing 'this' to SDL callbacks
    thread_local Sdl::Loop::Sdl3Runner* g_currentSdl3Runner = nullptr;
}

namespace Sdl::Loop
{
    template<auto MemberFunc>
    static auto SDLCALL MakeStatic(void* appstate, auto... args)
    {
        return (static_cast<Sdl::Loop::Sdl3Runner*>(appstate)->*MemberFunc)(args...);
    }

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

    int Sdl3Runner::Run()
    {
        Log::Debug(".");
        _updateCtx.Initialize();
        _running = true;

        // Pass instance to setup appstate in AppInit
        _selfRef = std::static_pointer_cast<Sdl3Runner>(shared_from_this());
        g_currentSdl3Runner = this;

        // Use SDL's cross-platform main loop implementation
        // This handles platform-specific details (macOS app delegate, emscripten, etc.)
        int result = SDL_EnterAppMainCallbacks(
            0, nullptr,  // argc/argv not needed, we already have them in Domain
            AppInit,
            MakeStatic<&Sdl3Runner::DoIterate>,
            MakeStatic<&Sdl3Runner::DoEvent>,
            MakeStatic<&Sdl3Runner::DoQuit>
        );
        Log::Trace("SDL_EnterAppMainCallbacks result {}", result);

#if __EMSCRIPTEN__
        // SDL_EnterAppMainCallbacks exits immediately in Emscripten
        //  but we need to wait for quit signal, so complete current execution flow
        Log::Trace("emscripten_exit_with_live_runtime()");
        emscripten_exit_with_live_runtime();
        __builtin_unreachable();
#endif

        auto exitResult = GetExitCode();
        auto exitCode = exitResult.value_or(SuccessExitCode);
        Log::Debug("exiting: {}", exitCode);
        return exitCode;
    }

    void Sdl3Runner::Exit(int exitCode)
    {
        Log::Debug("requested: {}", exitCode);
        SetExitCode(exitCode);
        _running = false;
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
        _window = Window{SDL_CreateWindow(
            _options.Window.Title.c_str(),
            _options.Window.Width,
            _options.Window.Height,
            _options.Window.Flags
        )};

        if (!_window) {
            Log::Error("SDL_CreateWindow failed: {}", SDL_GetError());
            return SDL_APP_FAILURE;
        }

        // Create renderer
        _renderer = Renderer{SDL_CreateRenderer(_window.get(), nullptr)};
        if (!_renderer) {
            Log::Error("SDL_CreateRenderer failed: {}", SDL_GetError());
            _window.reset();
            return SDL_APP_FAILURE;
        }

        // Set VSync
        if (!SDL_SetRenderVSync(_renderer.get(), _options.VSync)) {
            Log::Warn("SDL_SetRenderVSync({}) not supported, using disabled", _options.VSync);
            SDL_SetRenderVSync(_renderer.get(), SDL_RENDERER_VSYNC_DISABLED);
        }

        if (!InvokeStart()) {
            Log::Error("Started handler failed");
            _renderer.reset();
            _window.reset();
            return SDL_APP_FAILURE;
        }

        Log::Trace("completed");
        return SDL_APP_CONTINUE;
    }

    void Sdl3Runner::DoQuit(SDL_AppResult result)
    {
        auto exitResult = GetExitCode();
        int exitCode{};
        if (!exitResult) {
            // No exit code set yet - set based on SDL_AppResult
            const char* resultStr = boost::describe::enum_to_string(result, "Unknown");
            Log::Debug("quit result {}({})", resultStr, static_cast<int>(result));
            if (result == SDL_APP_SUCCESS) {
                exitCode = SuccessExitCode;
            } else {
                exitCode = FailureExitCode;
            }
            SetExitCode(exitCode);
        } else {
            exitCode = exitResult.value();
            Log::Debug("exit code {}", exitCode);
        }

        _running = false;
        InvokeStop();

        _renderer.reset();
        _window.reset();

#if __EMSCRIPTEN__
        // pospone runtime exit 
        // - allow application to handle quit event and cleanup
        // - to avoid issues with calling from inside SDL main loop
        // - allow runtime exit after SDL_AppQuit
        // Keep self alive during async callback by storing shared_ptr
        emscripten_async_call(
            [](void* state) {
                auto* runner = static_cast<Sdl3Runner*>(state);
                auto exitCode = runner->GetExitCode().value_or(SuccessExitCode);
                Log::Trace("emscripten_force_exit({})", exitCode);
                runner->_selfRef.reset(); // Release self-reference
                emscripten_force_exit(exitCode);
            },
            this,
            0
        );
#else
        _selfRef.reset();
#endif
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
        InvokeUpdate(_updateCtx);

        //TODO: if handler Update did not render anything - Default: clear with dark blue
        // SDL_SetRenderDrawColor(_renderer, 30, 30, 80, 255);
        // SDL_RenderClear(_renderer);

        SDL_RenderPresent(_renderer.get());
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
