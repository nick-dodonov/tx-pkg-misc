#pragma once

#include "PeerManager.h"

#include "imgui.h"
#include "SynTm/Types.h"
#include <string>

namespace IPeer
{
    /// Global control panel: peer creation, connection matrix, transport info, stats overview.
    class ControlPanel
    {
    public:
        void Render(PeerManager& mgr)
        {
            if (!ImGui::Begin("Control Panel")) {
                ImGui::End();
                return;
            }

            RenderTransportInfo(mgr);
            ImGui::Separator();
            RenderPeerCreation(mgr);
            ImGui::Separator();
            RenderPeerList(mgr);
            ImGui::Separator();
            RenderConnectionMatrix(mgr);
            ImGui::Separator();
            RenderStats(mgr);

            ImGui::End();
        }

    private:
        char _newPeerName[64] = {};

        void RenderTransportInfo(const PeerManager& mgr)
        {
            ImGui::Text("Transport: %s", TransportModeName(mgr.GetTransportMode()));
        }

        void RenderPeerCreation(PeerManager& mgr)
        {
            ImGui::Text("Create Peer");
            ImGui::SetNextItemWidth(160);
            ImGui::InputTextWithHint("##name", "Name (optional)", _newPeerName, sizeof(_newPeerName));
            ImGui::SameLine();
            if (ImGui::Button("Create")) {
                mgr.CreatePeer(_newPeerName);
                _newPeerName[0] = '\0';
            }
        }

        void RenderPeerList(const PeerManager& mgr)
        {
            if (ImGui::TreeNodeEx("Peers", ImGuiTreeNodeFlags_DefaultOpen)) {
                for (const auto& entry : mgr.Entries()) {
                    auto& peer = *entry.peer;
                    ImGui::PushID(peer.id);
                    ImGui::ColorButton("##color", peer.color, ImGuiColorEditFlags_NoTooltip, {12, 12});
                    ImGui::SameLine();

                    auto qualityStr = SynTm::SyncQualityToString(peer.consensus.Quality());
                    bool synced = peer.consensus.IsSynced();
                    ImGui::Text("%s (id=%d) [%s%s]",
                        peer.name.c_str(), peer.id,
                        synced ? "sync:" : "",
                        qualityStr.data());

                    ImGui::PopID();
                }
                if (mgr.Entries().empty()) {
                    ImGui::TextDisabled("No peers created yet");
                }
                ImGui::TreePop();
            }
        }

        void RenderConnectionMatrix(PeerManager& mgr)
        {
            if (!ImGui::TreeNodeEx("Connections", ImGuiTreeNodeFlags_DefaultOpen)) {
                return;
            }

            const auto& entries = mgr.Entries();
            for (size_t i = 0; i < entries.size(); ++i) {
                for (size_t j = i + 1; j < entries.size(); ++j) {
                    auto& a = *entries[i].peer;
                    auto& b = *entries[j].peer;
                    ImGui::PushID(static_cast<int>(i * 1000 + j));

                    bool connected = mgr.AreConnected(a.id, b.id);
                    if (connected) {
                        ImGui::ColorButton("##state", {0.26f, 0.85f, 0.42f, 1.0f},
                            ImGuiColorEditFlags_NoTooltip, {12, 12});
                        ImGui::SameLine();
                        ImGui::Text("%s <-> %s", a.name.c_str(), b.name.c_str());
                        ImGui::SameLine();
                        if (ImGui::SmallButton("Disconnect")) {
                            mgr.Disconnect(a, b);
                        }
                    } else {
                        ImGui::TextDisabled("  ");
                        ImGui::SameLine();
                        ImGui::Text("%s --- %s", a.name.c_str(), b.name.c_str());
                        ImGui::SameLine();
                        if (ImGui::SmallButton("Connect")) {
                            mgr.Connect(a, b);
                        }
                    }

                    ImGui::PopID();
                }
            }

            if (entries.size() < 2) {
                ImGui::TextDisabled("Need at least 2 peers");
            }
            ImGui::TreePop();
        }

        void RenderStats(const PeerManager& mgr)
        {
            if (!ImGui::TreeNodeEx("Stats", ImGuiTreeNodeFlags_DefaultOpen)) {
                return;
            }

            ImGui::Text("Peers: %zu", mgr.Entries().size());

            // Count total links and synced peers.
            int totalLinks = 0;
            int syncedPeers = 0;
            for (const auto& entry : mgr.Entries()) {
                totalLinks += static_cast<int>(entry.node->Links().size());
                if (entry.peer->consensus.IsSynced()) {
                    syncedPeers++;
                }
            }
            ImGui::Text("Links: %d", totalLinks / 2); // Each link counted twice.
            ImGui::Text("Synced peers: %d / %zu", syncedPeers, mgr.Entries().size());

            ImGui::TreePop();
        }
    };
}
