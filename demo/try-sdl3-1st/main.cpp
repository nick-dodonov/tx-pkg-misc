#include "Boot/Boot.h"
#include "Log/Log.h"

#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_version.h>

#ifdef __EMSCRIPTEN__
    #include <emscripten.h>
    #include <emscripten/html5.h>
#endif

// Global variables for emscripten loop
struct Context {
    SDL_Renderer* renderer;
    SDL_Window* window;
    bool running;
};

void mainLoop(void* arg)
{
    Context* ctx = static_cast<Context*>(arg);
    SDL_Event event;

    // Event handling
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
            ctx->running = false;
        }
        else if (event.type == SDL_EVENT_KEY_DOWN) {
            if (event.key.key == SDLK_ESCAPE) {
                ctx->running = false;
            }
        }

        if (ctx->running == false) {
#ifdef __EMSCRIPTEN__
            emscripten_cancel_main_loop();
#endif
            return;
        }
    }

    // Rendering
    SDL_SetRenderDrawColor(ctx->renderer, 30, 30, 130, 255);
    SDL_RenderClear(ctx->renderer);

    // Put your drawing code here
    SDL_SetRenderDrawColor(ctx->renderer, 255, 100, 100, 255);
    SDL_FRect rect = {50, 50, 100, 100};
    SDL_RenderFillRect(ctx->renderer, &rect);

    SDL_RenderPresent(ctx->renderer);
}

int main(int argc, const char** argv)
{
    Boot::LogHeader({argc, argv});
    Log::Info("SDL3 try demo 1st");
    int version = SDL_GetVersion();
    int major = SDL_VERSIONNUM_MAJOR(version);
    int minor = SDL_VERSIONNUM_MINOR(version);
    int patch = SDL_VERSIONNUM_MICRO(version);
    Log::Info("SDL version: {}.{}.{}", major, minor, patch);

    // 1. SDL3 initialization
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    // 2. Window creation
    SDL_Window* window = SDL_CreateWindow(
        "SDL3 Window",
        // 800, 600,
        // 600, 450,
        400,
        300,
        SDL_WINDOW_RESIZABLE
    );
    if (!window) {
        SDL_Log("Window creation failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    // 3. Renderer creation
    SDL_Renderer* renderer = SDL_CreateRenderer(window, NULL);
    if (!renderer) {
        SDL_Log("Renderer creation failed: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Create context for main loop
    Context context;
    context.renderer = renderer;
    context.window = window;
    context.running = true;

#ifdef __EMSCRIPTEN__
    // For emscripten use emscripten_set_main_loop_arg
    emscripten_set_main_loop_arg(mainLoop, &context, 0, true);
#else
    // For native version use a regular loop
    while (context.running) {
        Uint32 frameStart = SDL_GetTicks();

        mainLoop(&context);

        // FPS limit (only for native version)
        Uint32 frameTime = SDL_GetTicks() - frameStart;
        if (frameTime < 16) {
            SDL_Delay(16 - frameTime);
        }
    }
#endif

    // 5. Cleanup
    Log::Info("SDL Cleanup");
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
