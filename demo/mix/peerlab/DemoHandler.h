#pragma once
#include <algorithm>

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
        ImGuiID _dockIdLastSlot = 0; // last assigned peer slot, split-right for each new peer

    public:
        explicit DemoHandler(RunLoop::CompositeHandler& composite)
            : _peerManager(composite, _transportFactory)
            , _controlPanel(_peerManager)
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
            // First peer gets a 100s head start so the two initial peer clocks differ.
            auto& p0 = _peerManager.CreatePeer();
            p0.localClock.Advance(std::chrono::seconds{100});
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
                    if (ImGui::MenuItem("Reset Layout")) {
                        _dockingInitialized = false;
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
            _controlPanel.Render();

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
                win->Render(_peerManager);
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
                const auto* text = event.text.text;
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
                const auto hasWindow = std::ranges::any_of(_peerWindows,
                    [id = entry.peer->id](const auto& w) { return w->GetPeerId() == id; });
                if (!hasWindow) {
                    CreatePeerWindow(entry);
                }
            }
        }

        void CreatePeerWindow(const ManagedPeer& entry)
        {
            _peerWindows.emplace_back(std::make_unique<PeerWindow>(*entry.peer));

            // Place new peer to the right of the last slot.
            if (_dockingInitialized && _dockIdLastSlot != 0) {
                const auto splitDir = _dockIdLastSlot == _dockIdBottom ? ImGuiDir_Left: ImGuiDir_Right;
                ImGui::DockBuilderSplitNode(_dockIdLastSlot, splitDir, 0.5f, &_dockIdLastSlot, nullptr);
                ImGui::DockBuilderDockWindow(entry.peer->peerId.c_str(), _dockIdLastSlot);

                // Make layout for bottom windows equal width
                {
                    const auto* bottomNode = ImGui::DockBuilderGetNode(_dockIdBottom);
                    auto* lastNode = ImGui::DockBuilderGetNode(_dockIdLastSlot);

                    // find nodes that belongs to bottom dock
                    std::vector leaves = {lastNode};
                    for (const auto& peerWindow : _peerWindows) {
                        const auto* window = ImGui::FindWindowByName(peerWindow->GetPeer().peerId.c_str());
                        if (window != nullptr) {
                            auto* node = window->DockNode;
                            if (node && window->DockNodeIsVisible) {
                                //detect one of the parent node is bottomNode
                                auto hasBottomParent = false;
                                auto* n = node;
                                while (n->ParentNode != nullptr) {
                                    n = n->ParentNode;
                                    if (n == bottomNode) {
                                        hasBottomParent = true;
                                        break;
                                    }
                                }
                                if (hasBottomParent) {
                                    leaves.emplace_back(node);
                                }
                            }
                        }
                    }

                    // make all of them equal width
                    if (leaves.size() > 2) {
                        auto total = 0.0f;
                        for (const auto* node : leaves) {
                            total += node->SizeRef.x;
                        }
                        const auto equal = total / static_cast<float>(leaves.size());
                        for (auto* node : leaves) {
                            node->SizeRef.x = equal;
                        }
                    }
                }
            }
        }

        /// Set up a programmatic docking layout: top for ControlPanel, bottom for PeerWindows.
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

            ImGui::DockBuilderSplitNode(dockspaceId, ImGuiDir_Up, 0.4f, &_dockIdTop, &_dockIdBottom);

            ImGui::DockBuilderDockWindow(ControlPanel::WindowName, _dockIdTop);

            // Dock peer windows side-by-side: first in _dockIdBottom, each next splits right.
            _dockIdLastSlot = _dockIdBottom;

            ImGui::DockBuilderFinish(dockspaceId);
            _dockingInitialized = true;
        }
    };
} // namespace Demo
