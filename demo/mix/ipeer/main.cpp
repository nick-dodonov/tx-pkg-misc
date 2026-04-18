#include "Boot/Boot.h"
#include "Fs/System.h"
#include "Im/Console/QuakeConsole.h"
#include "Im/Deputy.h"
#include "Log/Log.h"
#include "Sdl/Loop/Sdl3Runner.h"

struct PeerWindow
{
    std::string name;
    explicit PeerWindow(std::string name = "Window") : name(std::move(name)) {}

    void Render(const RunLoop::UpdateCtx& ctx, const std::shared_ptr<Im::Deputy>& imDeputy) const
    {
        // auto* mainViewport = ImGui::GetMainViewport();
        // ImGui::SetNextWindowPos(ImVec2(mainViewport->Size.x / 10, 4 * mainViewport->Size.y / 10), ImGuiCond_FirstUseEver);
        auto flags = 0; //ImGuiWindowFlags_AlwaysAutoResize;
        if (ImGui::Begin(name.c_str(), nullptr, flags)) {
            ImGui::Text("Session Time: %.2f s", ctx.session.passedSeconds);
            //ImGui::Spring();
            ImGui::Spacing();
            ImGui::Text("Frame Index: %llu", static_cast<unsigned long long>(ctx.frame.index));
            ImGui::Text("Delta: %.3f ms", ctx.frame.deltaSeconds * 1000.0f);

            auto framerate = imDeputy->GetImGuiIO().Framerate;
            ImGui::Text("ImGUI FPS: %.3f ms/frame (%.1f FPS)", 1000.0f / framerate, framerate);
        }
        ImGui::End();
    }
};

struct ImHandler
    : RunLoop::Handler
    , Sdl::Loop::Sdl3Handler
{
    std::shared_ptr<Im::Deputy> _imDeputy;
    std::unique_ptr<Im::QuakeConsole> _console;

    bool Start() override
    {
        Log::Info("SDL3 Runner initialized");
        _imDeputy = std::make_shared<Im::Deputy>(Im::Deputy::Config{
            .window=GetWindow(),
            .renderer=GetRenderer(),
            .drive=Fs::System::MakeDefaultDrive(),
        });
        
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

    void Update(const RunLoop::UpdateCtx& ctx) override
    {
        auto* renderer = GetRenderer();

        // Clear with dark blue
        SDL_SetRenderDrawColor(renderer, 30, 30, 130, 255);
        SDL_RenderClear(renderer);

        _imDeputy->UpdateBegin();

        // Main sample windows for peers
        {
            if (ImGui::BeginMainMenuBar()) {
                if (ImGui::BeginMenu("Peer")) {
                    if (ImGui::MenuItem("New", "Ctrl+N")) {
                        //TODO: peer creation logic
                    }
                    ImGui::EndMenu();
                }
                ImGui::EndMainMenuBar();
            }
    
            PeerWindow window1{"peer1"};
            window1.Render(ctx, _imDeputy);
        }

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

        if (event.type == SDL_EVENT_QUIT) {
            Log::Debug("received SDL_EVENT_QUIT, quitting");
            return SDL_APP_SUCCESS;
        }
        if (event.type == SDL_EVENT_KEY_DOWN) {
            // Log::Trace("Key pressed: {}", static_cast<int>(event.key.key));
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
                .Title = "Hello Peer",
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
