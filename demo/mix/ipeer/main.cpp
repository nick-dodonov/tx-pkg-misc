#include "ControlPanel.h"
#include "PeerManager.h"
#include "PeerWindow.h"
#include "TransportFactory.h"

#include "Boot/Boot.h"
#include "Fs/System.h"
#include "Im/Console/QuakeConsole.h"
#include "Im/Deputy.h"
#include "Log/Log.h"
#include "RunLoop/CompositeHandler.h"
#include "Sdl/Loop/Sdl3Runner.h"

#include "imgui_internal.h"

/// ImGui/SDL handler — renders UI and drives peer simulation on the main thread.
struct ImHandler
    : RunLoop::Handler
    , Sdl::Loop::Sdl3Handler
{
    RunLoop::CompositeHandler& _composite;
    IPeer::TransportFactory _transportFactory;
    IPeer::PeerManager _peerManager;

    std::shared_ptr<Im::Deputy> _imDeputy;
    std::unique_ptr<Im::QuakeConsole> _console;
    IPeer::ControlPanel _controlPanel;
    std::vector<std::unique_ptr<IPeer::PeerWindow>> _peerWindows;

    bool _dockingInitialized = false;
    ImGuiID _dockIdTop = 0;
    ImGuiID _dockIdBottom = 0;

    explicit ImHandler(RunLoop::CompositeHandler& composite)
        : _composite(composite)
        , _peerManager(composite, _transportFactory)
    {}

    bool Start() override
    {
        Log::Info("SDL3 Runner initialized");
        _imDeputy = std::make_shared<Im::Deputy>(Im::Deputy::Config{
            .window = GetWindow(),
            .renderer = GetRenderer(),
            .drive = Fs::System::MakeDefaultDrive(),
        });

        _console = std::make_unique<Im::QuakeConsole>(false);
        _console->Initialize();

        // Create two initial peers for immediate demo.
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

        // Advance peer positions (physics on UI thread).
        _peerManager.Update(ctx.frame.deltaSeconds, ctx.session.passedSeconds);

        _imDeputy->UpdateBegin();

        // Set up docking layout on first frame.
        SetupDocking();

        // Main menu bar.
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

        // Control panel.
        if (_dockIdTop != 0) {
            ImGui::SetNextWindowDockID(_dockIdTop, ImGuiCond_FirstUseEver);
        }
        _controlPanel.Render(_peerManager);

        // Sync peer windows with peer list.
        SyncPeerWindows();

        // Render all peer windows.
        for (auto& win : _peerWindows) {
            if (_dockIdBottom != 0) {
                ImGui::SetNextWindowDockID(_dockIdBottom, ImGuiCond_FirstUseEver);
            }
            win->Render(_peerManager, ctx.session.passedSeconds);
        }

        // Remove closed windows.
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
    /// Set up programmatic docking layout: top 40% for ControlPanel, bottom 60% for PeerWindows.
    void SetupDocking()
    {
        if (_dockingInitialized) {
            return;
        }

        ImGuiID dockspaceId = _imDeputy->GetDockSpaceId();
        if (dockspaceId == 0) {
            return;
        }

        // Only set up if the dockspace has no saved layout yet.
        auto* dockNode = ImGui::DockBuilderGetNode(dockspaceId);
        if (dockNode && dockNode->ChildNodes[0] != nullptr) {
            _dockingInitialized = true;
            return;
        }

        ImGui::DockBuilderRemoveNode(dockspaceId);
        ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_PassthruCentralNode);
        ImGui::DockBuilderSetNodeSize(dockspaceId, ImGui::GetMainViewport()->Size);

        ImGui::DockBuilderSplitNode(dockspaceId, ImGuiDir_Up, 0.40f, &_dockIdTop, &_dockIdBottom);

        ImGui::DockBuilderDockWindow("Control Panel", _dockIdTop);

        // Dock existing peer windows into the bottom area.
        for (auto& win : _peerWindows) {
            auto* peer = _peerManager.FindPeer(win->GetPeerId());
            if (peer) {
                ImGui::DockBuilderDockWindow(peer->name.c_str(), _dockIdBottom);
            }
        }

        ImGui::DockBuilderFinish(dockspaceId);
        _dockingInitialized = true;
    }

    /// Ensure every peer has a corresponding window.
    void SyncPeerWindows()
    {
        for (const auto& entry : _peerManager.Entries()) {
            bool hasWindow = false;
            for (auto& win : _peerWindows) {
                if (win->GetPeerId() == entry.peer->id) {
                    hasWindow = true;
                    break;
                }
            }
            if (!hasWindow) {
                _peerWindows.push_back(std::make_unique<IPeer::PeerWindow>(*entry.peer));
            }
        }
    }
};

int main(const int argc, const char* argv[])
{
    Boot::DefaultInit(argc, argv);

    auto composite = std::make_shared<RunLoop::CompositeHandler>();
    auto imHandler = std::make_shared<ImHandler>(*composite);
    composite->Add(*imHandler);

    auto runner = std::make_shared<Sdl::Loop::Sdl3Runner>(
        composite,
        imHandler,
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
