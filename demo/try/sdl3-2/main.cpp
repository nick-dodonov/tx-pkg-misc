#include "Boot/Boot.h"
#include "Log/Log.h"

//TODO: simple wrapper as SDL3pp fails yet w/ clang and C++20
/**
template <typename T, void (*Deleter)(T*)>
struct SDL_Deleter {
    void operator()(T* ptr) const noexcept {
        if (ptr) Deleter(ptr);
    }
};
template <typename T, void (*Deleter)(T*)>
using SDL_Ptr = std::unique_ptr<T, SDL_Deleter<T, Deleter>>;

using Window = SDL_Ptr<SDL_Window, SDL_DestroyWindow>;
using Renderer = SDL_Ptr<SDL_Renderer, SDL_DestroyRenderer>;

Window window{SDL_CreateWindow("Title", 800, 600, 0)};
**/

/*
 * This example creates an SDL window and renderer, and then draws some
 * textures to it every frame.
 *
 * This code is public domain. Feel free to use it for any purpose!
 */
#define SDL_MAIN_USE_CALLBACKS 1 /* use the callbacks instead of main() */
#include <SDL3/SDL_main.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_pixels.h>

/* We will use this renderer to draw into this window every frame. */
static SDL_Window* window = NULL;
static SDL_Renderer* renderer = NULL;
static SDL_Texture* texture = NULL;
static int texture_width = 0;
static int texture_height = 0;

#define WINDOW_WIDTH 640
#define WINDOW_HEIGHT 480

#if __ANDROID__
void redirect_stdout_to_logcat(void);
#endif

/* This function runs once at startup. */
SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[])
{
#if __ANDROID__
    redirect_stdout_to_logcat();
#endif

    Boot::LogHeader({argc, argv});
    Log::Info("SDL3 try demo 1st");
    int version = SDL_GetVersion();
    int major = SDL_VERSIONNUM_MAJOR(version);
    int minor = SDL_VERSIONNUM_MINOR(version);
    int patch = SDL_VERSIONNUM_MICRO(version);
    Log::Info("SDL version: {}.{}.{}", major, minor, patch);

    ////////////////////////////////////////////////////////////////
    SDL_Surface* surface = NULL;
    char* bmp_path = NULL;

    SDL_SetAppMetadata("Example Renderer Textures", "1.0", "com.example.renderer-textures");

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        Log::Error("Couldn't initialize SDL: {}", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!SDL_CreateWindowAndRenderer(
            "examples/renderer/textures",
            WINDOW_WIDTH,
            WINDOW_HEIGHT,
            SDL_WINDOW_RESIZABLE,
            &window,
            &renderer
        )) {
        Log::Error("Couldn't create window/renderer: {}", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    /* Textures are pixel data that we upload to the video hardware for fast drawing. Lots of 2D
       engines refer to these as "sprites." We'll do a static texture (upload once, draw many
       times) with data from a bitmap file. */

    /* SDL_Surface is pixel data the CPU can access. SDL_Texture is pixel data the GPU can access.
       Load a .bmp into a surface, move it to a texture from there. */

    const char* bmp_file = "data/sample.bmp";
    Log::Info("Loading BMP from BasePath: {}", bmp_file);
    SDL_asprintf(&bmp_path, "%s%s", SDL_GetBasePath(), bmp_file); /* allocate a string of the full file path */

    // const char* bmp_file2 = "demo/try/sdl3-2/data/sample.bmp";
    // Log::Info("Loading BMP from CWD: {}", bmp_file2);
    // SDL_asprintf(&bmp_path, "%s", bmp_file2);

    Log::Info("Loading BMP file: {}", bmp_path);
    surface = SDL_LoadBMP(bmp_path);
    SDL_free(bmp_path); /* done with this, the file is loaded. */
    if (!surface) {
        Log::Error("Couldn't load bitmap: {}", SDL_GetError());
        // return SDL_APP_FAILURE;

        // Create magneta surface as fallback
        surface = SDL_CreateSurface(64, 64, SDL_PIXELFORMAT_RGBA8888);
        if (!surface) {
            Log::Error("Couldn't create fallback surface: {}", SDL_GetError());
            return SDL_APP_FAILURE;
        }
        const SDL_PixelFormatDetails *formatDetails = SDL_GetPixelFormatDetails(surface->format);
        auto magentaColor = SDL_MapRGBA(formatDetails, nullptr, 255, 0, 255, 255);
        SDL_FillSurfaceRect(surface, nullptr, magentaColor);
    }

    texture_width = surface->w;
    texture_height = surface->h;

    texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!texture) {
        Log::Error("Couldn't create static texture: {}", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_DestroySurface(surface); /* done with this, the texture has a copy of the pixels now. */

    return SDL_APP_CONTINUE; /* carry on with the program! */
}

/* This function runs once at shutdown. */
void SDL_AppQuit(void* appstate, SDL_AppResult result)
{
    if (texture) {
        SDL_DestroyTexture(texture);
    }
    /* SDL will clean up the window/renderer for us. */
}

/* This function runs when a new event (mouse input, keypresses, etc) occurs. */
SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event)
{
    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS; /* end the program, reporting success to the OS. */
    }
    return SDL_APP_CONTINUE; /* carry on with the program! */
}

/* This function runs once per frame, and is the heart of the program. */
SDL_AppResult SDL_AppIterate(void* appstate)
{
    SDL_FRect dst_rect;
    const Uint64 now = SDL_GetTicks();

    /* we'll have some textures move around over a few seconds. */
    const float direction = ((now % 2000) >= 1000) ? 1.0f : -1.0f;
    const float scale = ((float)(((int)(now % 1000)) - 500) / 500.0f) * direction;

    /* as you can see from this, rendering draws over whatever was drawn before it. */
    SDL_SetRenderDrawColor(renderer, 10, 40, 20, SDL_ALPHA_OPAQUE); /* black, full alpha */
    SDL_RenderClear(renderer);                                   /* start with a blank canvas. */

    /* Just draw the static texture a few times. You can think of it like a
       stamp, there isn't a limit to the number of times you can draw with it. */
    if (texture) {
        /* top left */
        auto scale1 = 2.5f;
        dst_rect.x = (100.0f * scale);
        dst_rect.y = 0.0f;
        dst_rect.w = (float)texture_width * scale1;
        dst_rect.h = (float)texture_height * scale1;
        SDL_RenderTexture(renderer, texture, NULL, &dst_rect);

        /* center this one. */
        auto scale2 = 2.f;
        dst_rect.x = ((float)(WINDOW_WIDTH - texture_width * scale2)) / 2.0f;
        dst_rect.y = ((float)(WINDOW_HEIGHT - texture_height * scale2)) / 2.0f;
        dst_rect.w = (float)texture_width * scale2;
        dst_rect.h = (float)texture_height * scale2;
        SDL_RenderTexture(renderer, texture, NULL, &dst_rect);

        /* bottom right. */
        auto scale3 = 1.8f;
        dst_rect.x = ((float)(WINDOW_WIDTH - texture_width * scale3)) - (100.0f * scale);
        dst_rect.y = (float)(WINDOW_HEIGHT - texture_height * scale3);
        dst_rect.w = (float)texture_width * scale3;
        dst_rect.h = (float)texture_height * scale3;
        SDL_RenderTexture(renderer, texture, NULL, &dst_rect);
    }

    SDL_RenderPresent(renderer); /* put it all on the screen! */

    return SDL_APP_CONTINUE; /* carry on with the program! */
}
