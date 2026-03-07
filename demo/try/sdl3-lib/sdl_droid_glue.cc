#if __ANDROID__
#define SDL_MAIN_HANDLED
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_log.h>

// binding w/ droid_glue
//TODO: Droid/RedirectStdout.h
void redirect_stdout_to_logcat(void);

int main(int argc, const char* argv[]);

extern "C" int SDLCALL SDL_main(int argc, char *argv[])
{
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "SDL_main: argc=%d", argc);

    redirect_stdout_to_logcat();
    return main(argc, (const char**)argv);
}
#endif
