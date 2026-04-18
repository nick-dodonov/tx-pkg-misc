#include "ControlPanel.h"
#include "PeerManager.h"
#include "PeerWindow.h"

#include "Boot/Boot.h"
#include "Fs/System.h"
#include "Im/Console/QuakeConsole.h"
#include "Im/Deputy.h"
#include "Log/Log.h"
#include "Sdl/Loop/Sdl3Runner.h"

struct ImHandler
    : RunLoop::Handler
    , Sdl::Loop::Sdl3Handler
{
    std::shared_ptr<Im::Deputy> _imDeputy;
    std::unique_ptr<Im::QuakeConsole> _console;

    IPeer::PeerManager _peerManager;
    IPeer::ControlPanel _controlPanel;
    std::vector<std::unique_ptr<IPeer::PeerWindow>> _peerWindows;

    bool Start() override
    {
        Log::Info("SDL3 Runner initialized");
        _imDeputy = std::make_shared<Im::Deputy>(Im::Deputy::Config{
            .window = GetWindow(),
            .renderer = GetRenderer(),
            .drive = Fs::System::MakeDefaultDrive(),
        });

        _console = std::make_unique<Im::QuakeConsole>(true);
        _console->Initialize();

        // Create two initial peers for immediate demo
        auto& p1 = _peerManager.CreatePeer();
        auto& p2 = _peerManager.CreatePeer();
        _peerWindows.push_back(std::make_unique<IPeer::PeerWindow>(p1));
        _peerWindows.push_back(std::make_unique<IPeer::PeerWindow>(p2));

        return true;
    }

    void Stop() override
    {
        Log::Info("SDL3 Runner quitting");
        _peerWindows.clear();
        _console.reset();
        _imDeputy.reset();
    }

    void Update(const RunLoop::UpdateCtx& ctx) override
    {
        auto* renderer = GetRenderer();
        SDL_SetRenderDrawColor(renderer, 30, 30, 50, 255);
        SDL_RenderClear(renderer);

        // Advance simulation
        _peerManager.Update(ctx.frame.deltaSeconds, ctx.session.passedSeconds);

        _imDeputy->UpdateBegin();

        // Main menu bar
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("Peers")) {
                if (ImGui::MenuItem("Create New", "Ctrl+N")) {
                    auto& peer = _peerManager.CreatePeer();
                    _peerWindows.push_back(std::make_unique<IPeer::PeerWindow>(peer));
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Quit", "Escape")) {
                    GetRunner()->Exit(0);
                }
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        // Control panel
        _controlPanel.Render(_peerManager);

        // Sync peer windows with peer list (create windows for new peers)
        SyncPeerWindows();

        // Render all peer windows
        for (auto& win : _peerWindows) {
            win->Render(_peerManager, ctx.session.passedSeconds);
        }

        // Remove closed windows
        std::erase_if(_peerWindows, [](const auto& w) { return w->WantClose(); });

        _console->Render();
        _imDeputy->UpdateEnd();
    }

    SDL_AppResult Sdl3Event(Sdl::Loop::Sdl3Runner& runner, const SDL_Event& event) override
    {
        if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_GRAVE) {
            _console->Toggle();
            return SDL_APP_CONTINUE;
        }

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
            if (event.key.key == SDLK_ESCAPE) {
                Log::Debug("ESC pressed, quitting");
                return SDL_APP_SUCCESS;
            }
        }
        return SDL_APP_CONTINUE;
    }

private:
    /// Ensure every peer has a corresponding window.
    void SyncPeerWindows()
    {
        for (const auto& peer : _peerManager.GetPeers()) {
            bool hasWindow = false;
            for (auto& win : _peerWindows) {
                if (win->GetPeerId() == peer->id) {
                    hasWindow = true;
                    break;
                }
            }
            if (!hasWindow) {
                _peerWindows.push_back(std::make_unique<IPeer::PeerWindow>(*peer));
            }
        }
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
                .Title = "IPeer",
                .Width = 1200,
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
