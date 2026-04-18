#pragma once
#include "ControlPanel.h"
#include "PeerManager.h"
#include "PeerWindow.h"
#include "TransportFactory.h"

#include "Fs/System.h"
#include "Im/Console/QuakeConsole.h"
#include "Im/Deputy.h"
#include "Log/Log.h"
#include "Sdl/Loop/Sdl3Runner.h"

#include "imgui_internal.h"

namespace Demo
{
    /// ImGui/SDL handler — renders UI and drives peer simulation on the main thread.
    class DemoHandler
        : public RunLoop::Handler
        , public Sdl::Loop::Sdl3Handler
    {
        TransportFactory _transportFactory;
        PeerManager _peerManager;

        std::shared_ptr<Im::Deputy> _imDeputy;
        std::unique_ptr<Im::QuakeConsole> _console;

        ControlPanel _controlPanel;
        std::vector<std::unique_ptr<PeerWindow>> _peerWindows;

        bool _dockingInitialized = false;
        ImGuiID _dockIdTop = 0;
        ImGuiID _dockIdBottom = 0;

    public:
        explicit DemoHandler(RunLoop::CompositeHandler& composite)
            : _peerManager(composite, _transportFactory)
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
            _peerManager.CreatePeer();
            _peerManager.CreatePeer();

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
                        _peerManager.CreatePeer();
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

            // Process window closures: remove peers whose window was closed.
            for (const auto& win : _peerWindows) {
                if (win->WantClose()) {
                    _peerManager.RemovePeer(win->GetPeerId());
                }
            }

            // Reconcile windows with model: create missing, remove orphaned.
            ReconcileWindows();

            // Render all peer windows.
            for (const auto& win : _peerWindows) {
                if (_dockIdBottom != 0) {
                    ImGui::SetNextWindowDockID(_dockIdBottom, ImGuiCond_FirstUseEver);
                }
                win->Render(_peerManager, ctx.session.passedSeconds);
            }

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
        /// Reconcile peer windows with PeerManager state each frame.
        /// Creates windows for new peers, removes windows for deleted peers.
        void ReconcileWindows()
        {
            // Remove windows whose peer no longer exists.
            std::erase_if(_peerWindows, [this](const auto& w) {
                return !_peerManager.FindPeer(w->GetPeerId());
            });

            // Create windows for peers that have no window yet.
            for (const auto& entry : _peerManager.Entries()) {
                bool hasWindow = std::any_of(_peerWindows.begin(), _peerWindows.end(),
                    [id = entry.peer->id](const auto& w) { return w->GetPeerId() == id; });
                if (!hasWindow) {
                    _peerWindows.push_back(std::make_unique<PeerWindow>(*entry.peer));
                }
            }
        }

        /// Set up a programmatic docking layout: top 40% for ControlPanel, bottom 60% for PeerWindows.
        void SetupDocking()
        {
            if (_dockingInitialized) {
                return;
            }

            const auto dockspaceId = _imDeputy->GetDockSpaceId();
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

            ImGui::DockBuilderDockWindow(ControlPanel::WindowName, _dockIdTop);

            // Dock existing peer windows into the bottom area.
            for (const auto& win : _peerWindows) {
                if (const auto* peer = _peerManager.FindPeer(win->GetPeerId())) {
                    ImGui::DockBuilderDockWindow(peer->name.c_str(), _dockIdBottom);
                }
            }

            ImGui::DockBuilderFinish(dockspaceId);
            _dockingInitialized = true;
        }
    };
} // namespace Demo
