#pragma once

#include "PeerManager.h"

#include "imgui.h"
#include <string>

namespace IPeer
{
    /// Global control panel: peer creation, connection matrix, stats overview.
    class ControlPanel
    {
    public:
        void Render(PeerManager& mgr)
        {
            ImGui::SetNextWindowSize({340, 400}, ImGuiCond_FirstUseEver);
            if (!ImGui::Begin("Control Panel")) {
                ImGui::End();
                return;
            }

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
                for (const auto& peer : mgr.GetPeers()) {
                    ImGui::PushID(peer->id);
                    ImGui::ColorButton("##color", peer->color, ImGuiColorEditFlags_NoTooltip, {12, 12});
                    ImGui::SameLine();
                    ImGui::Text("%s (id=%d)", peer->name.c_str(), peer->id);
                    ImGui::PopID();
                }
                if (mgr.GetPeers().empty()) {
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

            const auto& peers = mgr.GetPeers();
            for (size_t i = 0; i < peers.size(); ++i) {
                for (size_t j = i + 1; j < peers.size(); ++j) {
                    auto& a = *peers[i];
                    auto& b = *peers[j];
                    ImGui::PushID(static_cast<int>(i * 1000 + j));

                    if (auto* link = mgr.FindLink(a.id, b.id)) {
                        ImVec4 stateColor = StateColor(link->GetState());
                        ImGui::ColorButton("##state", stateColor, ImGuiColorEditFlags_NoTooltip, {12, 12});
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

            if (peers.size() < 2) {
                ImGui::TextDisabled("Need at least 2 peers");
            }
            ImGui::TreePop();
        }

        void RenderStats(const PeerManager& mgr)
        {
            if (!ImGui::TreeNodeEx("Stats", ImGuiTreeNodeFlags_DefaultOpen)) {
                return;
            }

            ImGui::Text("Peers: %zu", mgr.GetPeers().size());
            ImGui::Text("Links: %zu", mgr.GetLinks().size());

            // Average sync quality
            double totalQuality = 0.0;
            int qualityCount = 0;
            for (const auto& link : mgr.GetLinks()) {
                for (const auto& peer : mgr.GetPeers()) {
                    const auto& dir = link->GetDirectionFor(peer->id);
                    if (dir.quality > 0.0) {
                        totalQuality += dir.quality;
                        qualityCount++;
                    }
                }
            }
            if (qualityCount > 0) {
                ImGui::Text("Avg Quality: %.0f%%", (totalQuality / qualityCount) * 100.0);
            } else {
                ImGui::TextDisabled("Avg Quality: N/A");
            }

            ImGui::TreePop();
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
