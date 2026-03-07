#if __ANDROID__
#define SDL_MAIN_HANDLED
#include <SDL3/SDL_main.h>

// binding w/ droid_glue
void redirect_stdout_to_logcat(void);

// SDL_main is the entry point for SDL on Android, so we define it to call our main function
int main(int argc, const char* argv[]);

extern "C" int SDLCALL SDL_main(int argc, char *argv[])
{
    redirect_stdout_to_logcat();
    return main(argc, (const char**)argv);
}
#endif
