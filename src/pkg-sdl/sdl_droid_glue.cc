// Wrapper for SDL that allows using either the standard main() entry point or the callback-based entry points on Android, 
//   depending on whether the app defines main() or #define SDL_MAIN_USE_CALLBACKS with #include <SDL3/SDL_main.h>
// (look demo/try/sdl3-1 or demo/try/sdl3-2 for examples of both styles)

#if __ANDROID__
#define SDL_MAIN_HANDLED
#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_log.h>

// binding w/ droid_glue
//TODO: Droid/RedirectStdout.h
void redirect_stdout_to_logcat(void);

// Weak for the case of main() style - app overrides it.
// In case of SDL_MAIN_USE_CALLBACKS style it required to workaround linking error with droid_glue (it requires main() symbol)
#undef main
__attribute__((weak)) int main(int argc, char* argv[])
{
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "glue: main is not implemented in app!");
    return 1;
}

// Weak for the case of SDL_MAIN_USE_CALLBACKS style - app will declare own SDL_main in that case.
// In case of main() style it's used to call main() in application.
__attribute__((weak)) extern "C" int SDLCALL SDL_main(int argc, char *argv[])
{
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "glue: SDL_main: argc=%d", argc);

    redirect_stdout_to_logcat();
    return main(argc, argv);
}
#endif
