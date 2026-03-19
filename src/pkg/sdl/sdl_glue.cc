// Wrapper for SDL that allows using either the standard main() entry point or the callback-based entry points on Android,
//   depending on whether the app defines main() or #define SDL_MAIN_USE_CALLBACKS with #include <SDL3/SDL_main.h>
// (look demo/try/sdl3-1 or demo/try/sdl3-2 for examples of both styles)

#if __ANDROID__
#include "Droid/Glue.h"
#include "Droid/redirect_stdout.h"
#include "Log/Log.h"

#include <android/asset_manager_jni.h>

#define SDL_MAIN_HANDLED
#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_system.h>

namespace
{
    /// Implementation of Droid::Glue that retrieves the JNI environment and asset manager from SDL's Android integration.
    class SdlGlue: public Droid::Glue
    {
        JNIEnv* _mainEnv{};

        jobject _jAssetManagerRef;
        AAssetManager* _assetManager{};

    public:
        void Init()
        {
            SetInternal(this);

            auto* env = _mainEnv = (JNIEnv*)SDL_GetAndroidJNIEnv();

            // TODO: _assetManager = activity->assetManager;
            {
                auto* jActivity = (jobject)SDL_GetAndroidActivity();
                if (!jActivity) {
                    Log::Error("SDL_GetAndroidActivity failed: {}", SDL_GetError());
                    return;
                }
                auto* jActivityClass = env->GetObjectClass(jActivity);
                if (!jActivityClass) {
                    Log::Error("GetObjectClass failed");
                    return;
                }
                auto* getAssetsMethod = env->GetMethodID(jActivityClass, "getAssets", "()Landroid/content/res/AssetManager;");
                if (!getAssetsMethod) {
                    Log::Error("GetMethodID(getAssets) failed");
                    return;
                }

                jobject javaAssetManager = env->CallObjectMethod(jActivity, getAssetsMethod);
                if (!javaAssetManager) {
                    Log::Error("CallObjectMethod(getAssets) failed");
                    return;
                }
                _jAssetManagerRef = env->NewGlobalRef(javaAssetManager);
                _assetManager = AAssetManager_fromJava(env, _jAssetManagerRef);
                if (!_assetManager) {
                    Log::Error("AAssetManager_fromJava failed");
                    return;
                }
            }
        }

        ~SdlGlue()
        {
            if (_jAssetManagerRef) {
                //TODO: LOCAL JNIEnv for this thread?
                _mainEnv->DeleteGlobalRef(_jAssetManagerRef);
            }
        }

        // Droid::Glue
        [[nodiscard]] JNIEnv* GetMainJNIEnv() const override { return _mainEnv; }
        [[nodiscard]] AAssetManager* GetAssetManager() const override { return _assetManager; }
    };

    SdlGlue sdlGlue;
}

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
__attribute__((weak)) extern "C" int SDLCALL SDL_main(int argc, char* argv[])
{
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "glue: SDL_main: argc=%d", argc);
    redirect_stdout_to_logcat();

    sdlGlue.Init();

    return main(argc, argv);
}
#endif
