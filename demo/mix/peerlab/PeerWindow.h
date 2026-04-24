#pragma once

#include "PeerManager.h"

#include "imgui.h"
#include "SynTm/Types.h"
#include <chrono>

namespace Demo
{
    /// Per-peer ImGui window: canvas, connection table, sync diagnostics, controls.
    class PeerWindow
    {
    public:
        explicit PeerWindow(Peer& peer) : _peer(peer) {}

        [[nodiscard]] int GetPeerId() const { return _peer.id; }

        void Render(PeerManager& mgr)
        {
            if (_firstRender) {
                ImGui::SetNextWindowFocus();
                _firstRender = false;
            }
            bool open = true;
            if (!ImGui::Begin(_peer.name.c_str(), &open)) {
                ImGui::End();
                _wantClose = !open;
                return;
            }
            _wantClose = !open;

            auto* node = mgr.FindNode(_peer.id);

            ImGui::BeginGroup();
            RenderCanvas(node);
            ImGui::EndGroup();

            ImGui::SameLine();

            ImGui::BeginGroup();
            RenderPeerState();
            ImGui::EndGroup();

            ImGui::Separator();
            RenderConnections(node);
            ImGui::Separator();
            RenderSyncDiagnostics();
            ImGui::Separator();
            RenderControls(mgr, node);

            ImGui::End();
        }

        [[nodiscard]] bool WantClose() const { return _wantClose; }

    private:
        Peer& _peer;
        bool _wantClose = false;
        bool _firstRender = true;

        /// Map normalized [-0.5, 0.5] coordinates to canvas pixel coordinates.
        static ImVec2 ToCanvas(const Vec2 pos, const ImVec2 canvasMin, const ImVec2 canvasSize)
        {
            return {
                canvasMin.x + (pos.x + 0.5f) * canvasSize.x,
                canvasMin.y + (pos.y + 0.5f) * canvasSize.y,
            };
        }

        void RenderCanvas(const PeerNode* node) const
        {
            ImVec2 canvasSize{100, 100};
            ImVec2 canvasMin = ImGui::GetCursorScreenPos();

            auto* drawList = ImGui::GetWindowDrawList();
            ImVec2 canvasMax{canvasMin.x + canvasSize.x, canvasMin.y + canvasSize.y};
            drawList->AddRectFilled(canvasMin, canvasMax, IM_COL32(20, 20, 40, 255));
            drawList->AddRect(canvasMin, canvasMax, IM_COL32(80, 80, 100, 255));

            // Grid
            for (int i = 1; i < 4; ++i) {
                float x = canvasMin.x + canvasSize.x * (static_cast<float>(i) / 4.0f);
                float y = canvasMin.y + canvasSize.y * (static_cast<float>(i) / 4.0f);
                drawList->AddLine({x, canvasMin.y}, {x, canvasMax.y}, IM_COL32(40, 40, 60, 255));
                drawList->AddLine({canvasMin.x, y}, {canvasMax.x, y}, IM_COL32(40, 40, 60, 255));
            }

            // Draw remote peer positions from connected links (dead-reckoning).
            if (node) {
                for (const auto& [remotePeerId, ls] : node->Links()) {
                    if (!ls.hasPayload) {
                        continue;
                    }
                    //TODO: Dead-reckoning extrapolation when dt will be correctly calculated
                    //auto syncNow = _peer.syncClock.Now();
                    // auto dt = static_cast<float>(
                    //     std::chrono::duration<double>(syncNow - SynTm::Ticks{ls.lastPayload.syncTimeNs}).count());
                    Vec2 remotePos{
                        .x = ls.lastPayload.x,// + ls.lastPayload.vx * dt,
                        .y = ls.lastPayload.y,// + ls.lastPayload.vy * dt,
                    };
                    ImVec2 px = ToCanvas(remotePos, canvasMin, canvasSize);
                    drawList->AddCircleFilled(px, 5.0f, IM_COL32(200, 200, 200, 200));
                    drawList->AddText({px.x + 7, px.y - 6}, IM_COL32(200, 200, 200, 200),
                        remotePeerId.c_str());
                }
            }

            // Draw local peer position (larger, on top).
            {
                ImVec2 px = ToCanvas(_peer.position, canvasMin, canvasSize);
                ImU32 col = ImGui::ColorConvertFloat4ToU32(_peer.color);
                drawList->AddCircleFilled(px, 8.0f, col);
                drawList->AddCircle(px, 8.0f, IM_COL32(255, 255, 255, 180), 0, 1.5f);
            }

            ImGui::Dummy(canvasSize);
        }

        void RenderPeerState() const
        {
            ImGui::Text("peerId: %s", _peer.peerId.c_str());
            ImGui::Text("position: (%.3f, %.3f)", _peer.position.x, _peer.position.y);
            ImGui::Text("velocity: (%.3f, %.3f)", _peer.velocity.x, _peer.velocity.y);

            auto localNow = _peer.clock.Now();
            double localSeconds = std::chrono::duration<double>(localNow).count();
            ImGui::Text("local: %.3f s", localSeconds);

            auto syncNow = _peer.syncClock.Now();
            double syncSeconds = std::chrono::duration<double>(syncNow).count();
            ImGui::Text("sync: %.3f s", syncSeconds);
        }

        void RenderConnections(PeerNode* node) const
        {
            if (!node || node->Links().empty()) {
                ImGui::TextDisabled("No connections");
                return;
            }

            if (ImGui::BeginTable("##conns", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders)) {
                ImGui::TableSetupColumn("Remote");
                ImGui::TableSetupColumn("Connected");
                ImGui::TableSetupColumn("Has Payload");
                ImGui::TableSetupColumn("Sync Quality");
                ImGui::TableHeadersRow();

                for (const auto& [remotePeerId, ls] : node->Links()) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::Text("%s", remotePeerId.c_str());

                    ImGui::TableNextColumn();
                    ImGui::TextColored(
                        ls.link ? ImVec4{0.26f, 0.85f, 0.42f, 1.0f} : ImVec4{0.98f, 0.39f, 0.26f, 1.0f},
                        "%s", ls.link ? "yes" : "no");

                    ImGui::TableNextColumn();
                    ImGui::Text("%s", ls.hasPayload ? "yes" : "no");

                    ImGui::TableNextColumn();
                    auto* session = _peer.consensus.GetSession(remotePeerId);
                    if (session) {
                        auto q = session->Quality();
                        ImGui::Text("%s", SynTm::SyncQualityToString(q).data());
                    } else {
                        ImGui::TextDisabled("N/A");
                    }
                }
                ImGui::EndTable();
            }
        }

        void RenderSyncDiagnostics() const
        {
            if (!ImGui::TreeNodeEx("Sync Diagnostics", ImGuiTreeNodeFlags_DefaultOpen)) {
                return;
            }

            bool synced = _peer.consensus.IsSynced();
            auto quality = _peer.consensus.Quality();

            ImGui::Text("Synced: %s", synced ? "yes" : "no");
            ImGui::Text("Quality: %s", SynTm::SyncQualityToString(quality).data());
            ImGui::Text("Peer count: %zu", _peer.consensus.PeerCount());

            const auto& epoch = _peer.consensus.Epoch();
            if (epoch.IsValid()) {
                ImGui::Text("Epoch ID: %llu", static_cast<unsigned long long>(epoch.id));
                ImGui::Text("Epoch members: %u", epoch.memberCount);
            } else {
                ImGui::TextDisabled("No epoch");
            }

            ImGui::TreePop();
        }

        void RenderControls(const PeerManager& mgr, PeerNode* node) const
        {
            if (!ImGui::TreeNodeEx("Controls", ImGuiTreeNodeFlags_DefaultOpen)) {
                return;
            }

            const auto& entries = mgr.Entries();
            bool hasUnconnected = false;
            for (const auto& entry : entries) {
                if (entry.peer->id != _peer.id && !mgr.AreConnected(_peer.id, entry.peer->id)) {
                    hasUnconnected = true;
                    break;
                }
            }

            if (hasUnconnected) {
                for (const auto& entry : entries) {
                    if (entry.peer->id == _peer.id || mgr.AreConnected(_peer.id, entry.peer->id)) {
                        continue;
                    }
                    ImGui::PushID(entry.peer->id);
                    char label[64];
                    std::snprintf(label, sizeof(label), "Connect to %s", entry.peer->name.c_str());
                    if (ImGui::SmallButton(label)) {
                        mgr.Connect(_peer, *entry.peer);
                    }
                    ImGui::PopID();
                }
            } else if (entries.size() > 1) {
                ImGui::TextDisabled("Connected to all peers");
            }

            // Disconnect buttons for existing links.
            if (node) {
                for (const auto& [remotePeerId, ls] : node->Links()) {
                    if (!ls.link) {
                        continue;
                    }
                    ImGui::PushID(remotePeerId.c_str());
                    char label[64];
                    std::snprintf(label, sizeof(label), "Disconnect %s", remotePeerId.c_str());
                    if (ImGui::SmallButton(label)) {
                        node->DisconnectFrom(remotePeerId);
                    }
                    ImGui::PopID();
                }
            }

            ImGui::TreePop();
        }
    };
}
