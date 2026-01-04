#include "Sdl/Loop/Sdl3Runner.h"
#include "App/Domain.h"
#include "Log/Log.h"
#include <cmath>
#include <memory>

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"

namespace asio = boost::asio;

namespace {
    std::shared_ptr<App::Domain> domain;
    ImGuiIO* imGuiIO = nullptr;
    bool show_demo_window = true;
}

static asio::awaitable<int> CoroMain()
{
    auto executor = co_await asio::this_coro::executor;
    auto runner = domain->GetRunner<Sdl::Loop::Sdl3Runner>();

    // Wait for EITHER quit event OR timer - whichever comes first
    Log::Info("waiting for quit event");
    co_await runner->WaitForQuit();

    Log::Info("exiting");
    co_return 0;
}

struct ImHandler : Sdl::Loop::Sdl3Handler
{
    bool Started(Sdl::Loop::Sdl3Runner& runner) override
    { 
        Log::Info("SDL3 Runner initialized");
        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        imGuiIO = &io;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        // Setup Dear ImGui style
        ImGui::StyleColorsDark();

        // Setup scaling
        float main_scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
        ImGuiStyle& style = ImGui::GetStyle();
        style.ScaleAllSizes(main_scale);        // Bake a fixed style scale. (until we have a solution for dynamic style scaling, changing this requires resetting Style + calling this again)
        style.FontScaleDpi = main_scale;        // Set initial font scale. (using io.ConfigDpiScaleFonts=true makes this unnecessary. We leave both here for documentation purpose)

        // Setup Platform/Renderer backends
        ImGui_ImplSDL3_InitForSDLRenderer(runner.GetWindow(), runner.GetRenderer());
        ImGui_ImplSDLRenderer3_Init(runner.GetRenderer());
        return true;
    }

    void Stopping(Sdl::Loop::Sdl3Runner& runner) override
    {
        Log::Info("SDL3 Runner quitting");
        // Cleanup
        ImGui_ImplSDLRenderer3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();

        imGuiIO = nullptr;
    }

    bool Update(Sdl::Loop::Sdl3Runner& runner, const App::Loop::UpdateCtx& ctx) override
    {
        auto* renderer = runner.GetRenderer();
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
        SDL_FRect rect = {x - size/2, y - size/2, size, size};
        SDL_RenderFillRect(renderer, &rect);

        // Second rectangle rotating opposite direction
        float x2 = centerX + radius * std::cosf(-elapsed * 1.5f);
        float y2 = centerY + radius * std::sinf(-elapsed * 1.5f);
        SDL_SetRenderDrawColor(renderer, 100, 200, 100, 255);
        SDL_FRect rect2 = {x2 - 25, y2 - 25, 50, 50};
        SDL_RenderFillRect(renderer, &rect2);

        // ImGui start frame
        {
            ImGui_ImplSDLRenderer3_NewFrame();
            ImGui_ImplSDL3_NewFrame();
            ImGui::NewFrame();
        }

        // ImGui sample windows
        {
            // sample window
            ImGui::Begin("Hello, world!");

            ImGui::Checkbox("Demo Window", &show_demo_window);
            ImGui::Text("Session Time: %.2f s", ctx.session.passedSeconds);
            ImGui::Text("Frame Index: %llu", static_cast<unsigned long long>(ctx.frame.index));
            ImGui::Text("Delta: %.3f ms", ctx.frame.deltaSeconds * 1000.0f);

            ImGui::Text("ImGUI FPS: %.3f ms/frame (%.1f FPS)", 1000.0f / imGuiIO->Framerate, imGuiIO->Framerate);
            ImGui::End();

            // default demo window
            if (show_demo_window) {
                ImGui::SetNextWindowPos(ImVec2(50, 20), ImGuiCond_FirstUseEver);
                ImGui::ShowDemoWindow(&show_demo_window);
            }
        }

        // ImGui rendering
        {
            ImGui::Render();
            SDL_SetRenderScale(renderer, imGuiIO->DisplayFramebufferScale.x, imGuiIO->DisplayFramebufferScale.y);
            ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
        }
        return true; 
    }

    void Sdl3Event(Runner& runner, const SDL_Event& event) override
    {
        Log::Trace("type={}", static_cast<int>(event.type));
        if (event.type == SDL_EVENT_KEY_DOWN) {
            Log::Debug("SDL_EVENT_KEY_DOWN: {}", static_cast<int>(event.key.key));
        }

        // ImGui input handling
        ImGui_ImplSDL3_ProcessEvent(&event);
    }
};

int main(const int argc, const char* argv[])
{
    auto runner = std::make_shared<Sdl::Loop::Sdl3Runner>(
        Sdl::Loop::Sdl3Runner::Options{
            .Window = {
                .Title = "Hello ImGUI",
                .Width = 1000,
                .Height = 800,
            },
        }
    );
    runner->Start(std::make_shared<ImHandler>());

    // // Create domain with custom runner
    // domain = std::make_shared<App::Domain>(argc, argv, runner);
    // return domain->RunCoroMain(CoroMain());
}
