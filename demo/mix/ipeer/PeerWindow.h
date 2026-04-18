#pragma once

#include "PeerManager.h"

#include "imgui.h"
#include <cstdio>

namespace IPeer
{
    /// Per-peer ImGui window: canvas, connections table, diagnostics, controls.
    class PeerWindow
    {
    public:
        explicit PeerWindow(Peer& peer) : _peer(peer) {}

        [[nodiscard]] int GetPeerId() const { return _peer.id; }

        void Render(PeerManager& mgr, double sessionTime)
        {
            bool open = true;
            //ImGui::SetNextWindowSize({360, 480}, ImGuiCond_FirstUseEver);
            if (!ImGui::Begin(_peer.name.c_str(), &open)) {
                ImGui::End();
                _wantClose = !open;
                return;
            }
            _wantClose = !open;

            RenderCanvas(mgr, sessionTime);
            ImGui::Separator();
            RenderConnections(mgr);
            ImGui::Separator();
            RenderDiagnostics(mgr);
            ImGui::Separator();
            RenderControls(mgr);

            ImGui::End();
        }

        [[nodiscard]] bool WantClose() const { return _wantClose; }

    private:
        Peer& _peer;
        bool _wantClose = false;

        /// Map normalized [-0.5, 0.5] coordinates to canvas pixel coordinates.
        static ImVec2 ToCanvas(Vec2 pos, ImVec2 canvasMin, ImVec2 canvasSize)
        {
            return {
                canvasMin.x + (pos.x + 0.5f) * canvasSize.x,
                canvasMin.y + (pos.y + 0.5f) * canvasSize.y,
            };
        }

        void RenderCanvas(PeerManager& mgr, double sessionTime) const
        {
            ImVec2 canvasSize{200, 200};
            ImVec2 canvasMin = ImGui::GetCursorScreenPos();

            // Draw canvas background
            auto* drawList = ImGui::GetWindowDrawList();
            ImVec2 canvasMax{canvasMin.x + canvasSize.x, canvasMin.y + canvasSize.y};
            drawList->AddRectFilled(canvasMin, canvasMax, IM_COL32(20, 20, 40, 255));
            drawList->AddRect(canvasMin, canvasMax, IM_COL32(80, 80, 100, 255));

            // Draw grid
            for (int i = 1; i < 4; ++i) {
                float x = canvasMin.x + canvasSize.x * (static_cast<float>(i) / 4.0f);
                float y = canvasMin.y + canvasSize.y * (static_cast<float>(i) / 4.0f);
                drawList->AddLine({x, canvasMin.y}, {x, canvasMax.y}, IM_COL32(40, 40, 60, 255));
                drawList->AddLine({canvasMin.x, y}, {canvasMax.x, y}, IM_COL32(40, 40, 60, 255));
            }

            // Draw remote peer positions from connected links
            auto links = mgr.GetLinksFor(_peer.id);
            for (auto* link : links) {
                auto& remote = link->GetRemotePeer(_peer.id);
                Vec2 remotePos = link->EstimateRemotePosition(_peer.id, sessionTime);
                ImVec2 px = ToCanvas(remotePos, canvasMin, canvasSize);
                ImU32 col = ImGui::ColorConvertFloat4ToU32(remote.color);
                drawList->AddCircleFilled(px, 5.0f, col);
                // Label
                drawList->AddText({px.x + 7, px.y - 6}, col, remote.name.c_str());
            }

            // Draw local peer position (larger, on top)
            {
                ImVec2 px = ToCanvas(_peer.position, canvasMin, canvasSize);
                ImU32 col = ImGui::ColorConvertFloat4ToU32(_peer.color);
                drawList->AddCircleFilled(px, 8.0f, col);
                drawList->AddCircle(px, 8.0f, IM_COL32(255, 255, 255, 180), 0, 1.5f);
            }

            // Reserve the canvas space
            ImGui::Dummy(canvasSize);
        }

        void RenderConnections(PeerManager& mgr) const
        {
            auto links = mgr.GetLinksFor(_peer.id);
            if (links.empty()) {
                ImGui::TextDisabled("No connections");
                return;
            }

            if (ImGui::BeginTable("##conns", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders)) {
                ImGui::TableSetupColumn("Peer");
                ImGui::TableSetupColumn("State");
                ImGui::TableSetupColumn("RTT");
                ImGui::TableSetupColumn("Offset");
                ImGui::TableSetupColumn("Quality");
                ImGui::TableHeadersRow();

                for (auto* link : links) {
                    auto& remote = link->GetRemotePeer(_peer.id);
                    const auto& dir = link->GetDirectionFor(_peer.id);

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::ColorButton("##c", remote.color, ImGuiColorEditFlags_NoTooltip, {10, 10});
                    ImGui::SameLine();
                    ImGui::Text("%s", remote.name.c_str());

                    ImGui::TableNextColumn();
                    ImVec4 stateCol = StateColor(link->GetState());
                    ImGui::TextColored(stateCol, "%s", StateName(link->GetState()));

                    ImGui::TableNextColumn();
                    ImGui::Text("%.1f ms", dir.rtt * 1000.0);

                    ImGui::TableNextColumn();
                    ImGui::Text("%.2f ms", dir.offset * 1000.0);

                    ImGui::TableNextColumn();
                    ImGui::Text("%.0f%%", dir.quality * 100.0);
                }
                ImGui::EndTable();
            }
        }

        void RenderDiagnostics(PeerManager& mgr) const
        {
            if (!ImGui::TreeNode("Diagnostics")) {
                return;
            }

            ImGui::Text("Position: (%.3f, %.3f)", _peer.position.x, _peer.position.y);
            ImGui::Text("Velocity: (%.3f, %.3f)", _peer.velocity.x, _peer.velocity.y);

            auto links = mgr.GetLinksFor(_peer.id);
            for (auto* link : links) {
                auto& remote = link->GetRemotePeer(_peer.id);
                const auto& dir = link->GetDirectionFor(_peer.id);
                if (ImGui::TreeNode(remote.name.c_str())) {
                    ImGui::Text("Probes sent: %d", dir.probesSent);
                    ImGui::Text("Probes received: %d", dir.probesReceived);
                    ImGui::Text("Messages sent: %d", dir.messagesSent);
                    ImGui::Text("Messages received: %d", dir.messagesReceived);
                    ImGui::Text("Has payload: %s", dir.hasPayload ? "yes" : "no");
                    if (dir.hasPayload) {
                        ImGui::Text("Last payload pos: (%.3f, %.3f)",
                            dir.lastPayload.x, dir.lastPayload.y);
                    }
                    ImGui::TreePop();
                }
            }
            ImGui::TreePop();
        }

        void RenderControls(PeerManager& mgr)
        {
            if (!ImGui::TreeNodeEx("Controls", ImGuiTreeNodeFlags_DefaultOpen)) {
                return;
            }

            // Quick-connect combo to unconnected peers
            const auto& peers = mgr.GetPeers();
            bool hasUnconnected = false;
            for (const auto& other : peers) {
                if (other->id != _peer.id && !mgr.AreConnected(_peer.id, other->id)) {
                    hasUnconnected = true;
                    break;
                }
            }

            if (hasUnconnected) {
                for (const auto& other : peers) {
                    if (other->id == _peer.id || mgr.AreConnected(_peer.id, other->id)) {
                        continue;
                    }
                    ImGui::PushID(other->id);
                    char label[64];
                    std::snprintf(label, sizeof(label), "Connect to %s", other->name.c_str());
                    if (ImGui::SmallButton(label)) {
                        mgr.Connect(_peer, *other);
                    }
                    ImGui::PopID();
                }
            } else if (peers.size() > 1) {
                ImGui::TextDisabled("Connected to all peers");
            }

            // Disconnect buttons for existing links
            auto links = mgr.GetLinksFor(_peer.id);
            for (auto* link : links) {
                auto& remote = link->GetRemotePeer(_peer.id);
                ImGui::PushID(remote.id + 10000);
                char label[64];
                std::snprintf(label, sizeof(label), "Disconnect %s", remote.name.c_str());
                if (ImGui::SmallButton(label)) {
                    mgr.Disconnect(_peer, remote);
                }
                ImGui::PopID();
            }

            ImGui::TreePop();
        }

        static const char* StateName(LinkState state)
        {
            switch (state) {
                case LinkState::Connected: return "Connected";
                case LinkState::Syncing:   return "Syncing";
                case LinkState::Synced:    return "Synced";
            }
            return "Unknown";
        }

        static ImVec4 StateColor(LinkState state)
        {
            switch (state) {
                case LinkState::Connected: return {0.5f, 0.5f, 0.5f, 1.0f};
                case LinkState::Syncing:   return {0.95f, 0.75f, 0.2f, 1.0f};
                case LinkState::Synced:    return {0.26f, 0.85f, 0.42f, 1.0f};
            }
            return {1, 1, 1, 1};
        }
    };
}
