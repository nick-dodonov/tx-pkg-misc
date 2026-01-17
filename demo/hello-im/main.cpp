#include "Boot/Boot.h"
#include "Log/Log.h"
#include "Sdl/Loop/Sdl3Runner.h"
#include "Im/Deputy.h"
#include "Im/Console/QuakeConsole.h"

namespace
{
    bool show_demo_window = true;
}

struct ImHandler
    : App::Loop::Handler
    , Sdl::Loop::Sdl3Handler
{
    std::shared_ptr<Im::Deputy> _imDeputy;
    std::unique_ptr<Im::QuakeConsole> _console;
    bool Start() override
    {
        Log::Info("SDL3 Runner initialized");
        auto& sdlRunner = *static_cast<Sdl::Loop::Sdl3Runner*>(GetRunner());
        _imDeputy = std::make_shared<Im::Deputy>(sdlRunner.GetWindow(), sdlRunner.GetRenderer());
        
        // Initialize Quake-style console (visible by default)
        _console = std::make_unique<Im::QuakeConsole>(true);
        _console->Initialize();
        
        return true;
    }

    void Stop() override
    {
        Log::Info("SDL3 Runner quitting");
        _console.reset();
        _imDeputy.reset();
    }

    void Update(const App::Loop::UpdateCtx& ctx) override
    {
        auto& sdlRunner = static_cast<Sdl::Loop::Sdl3Runner&>(ctx.Runner);
        auto* renderer = sdlRunner.GetRenderer();
        auto elapsed = ctx.session.passedSeconds;

        // Clear with dark blue
        SDL_SetRenderDrawColor(renderer, 30, 30, 130, 255);
        SDL_RenderClear(renderer);

        // Animated rectangle - moves in circle and pulses
        float centerX = 320.0f;
        float centerY = 240.0f;
        float radius = 100.0f;
        float x = centerX + radius * std::cosf(elapsed * 2.0f);
        float y = centerY + radius * std::sinf(elapsed * 2.0f);
        float size = 50.0f + 20.0f * std::sinf(elapsed * 4.0f);

        // Color cycles through red/orange
        auto r = static_cast<Uint8>(200 + 55 * std::sinf(elapsed * 3.0f));
        auto g = static_cast<Uint8>(80 + 40 * std::sinf(elapsed * 2.0f));

        SDL_SetRenderDrawColor(renderer, r, g, 50, 255);
        SDL_FRect rect = {x - size / 2, y - size / 2, size, size};
        SDL_RenderFillRect(renderer, &rect);

        // Second rectangle rotating opposite direction
        float x2 = centerX + radius * std::cosf(-elapsed * 1.5f);
        float y2 = centerY + radius * std::sinf(-elapsed * 1.5f);
        SDL_SetRenderDrawColor(renderer, 100, 200, 100, 255);
        SDL_FRect rect2 = {x2 - 25, y2 - 25, 50, 50};
        SDL_RenderFillRect(renderer, &rect2);

        _imDeputy->UpdateBegin();

        // ImGui sample windows
        {
            // sample window
            ImGui::Begin("Hello, world!");

            ImGui::Checkbox("Demo Window", &show_demo_window);
            ImGui::Text("Session Time: %.2f s", ctx.session.passedSeconds);
            ImGui::Text("Frame Index: %llu", static_cast<unsigned long long>(ctx.frame.index));
            ImGui::Text("Delta: %.3f ms", ctx.frame.deltaSeconds * 1000.0f);

            auto framerate = _imDeputy->GetImGuiIO().Framerate;
            ImGui::Text("ImGUI FPS: %.3f ms/frame (%.1f FPS)", 1000.0f / framerate, framerate);
            ImGui::End();

            // default demo window
            if (show_demo_window) {
                ImGui::SetNextWindowPos(ImVec2(50, 20), ImGuiCond_FirstUseEver);
                ImGui::ShowDemoWindow(&show_demo_window);
            }
        }

        // Quake-style console
        _console->Render();

        _imDeputy->UpdateEnd();
    }

    SDL_AppResult Sdl3Event(Sdl::Loop::Sdl3Runner& runner, const SDL_Event& event) override
    {
        // Handle console toggle before ImGui to prevent ` from appearing in input
        if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_GRAVE) {
            _console->Toggle();
            return SDL_APP_CONTINUE;
        }
        
        // Block text input for ` character always (to prevent it from appearing anywhere)
        if (event.type == SDL_EVENT_TEXT_INPUT) {
            const char* text = event.text.text;
            if (text && (text[0] == '`' || text[0] == '~')) {
                return SDL_APP_CONTINUE;
            }
        }

        _imDeputy->ProcessSdlEvent(event);

        // Log::Trace("type={}", static_cast<int>(event.type));
        if (event.type == SDL_EVENT_QUIT) {
            Log::Debug("received SDL_EVENT_QUIT");
            return SDL_APP_SUCCESS;
        }
        if (event.type == SDL_EVENT_KEY_DOWN) {
            Log::Trace("Key pressed: {}", static_cast<int>(event.key.key));
            if (event.key.key == SDLK_ESCAPE) {
                Log::Debug("ESC pressed, quitting");
                return SDL_APP_SUCCESS;
            }
        }
        return SDL_APP_CONTINUE;
    }
};

int main(const int argc, const char* argv[])
{
    Boot::DefaultInit(argc, argv);
    auto handler = std::make_shared<ImHandler>();
    auto runner = std::make_shared<Sdl::Loop::Sdl3Runner>(
        handler,
        handler,
        Sdl::Loop::Sdl3Runner::Options{
            .Window = {
                .Title = "Hello ImGUI",
                .Width = 1000,
                .Height = 800,
                .Flags = 
                    SDL_WINDOW_RESIZABLE 
                    | SDL_WINDOW_HIGH_PIXEL_DENSITY 
                    | SDL_WINDOW_FILL_DOCUMENT,
            },
        }
    );
    return runner->Run();
}
