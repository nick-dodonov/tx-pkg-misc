#pragma once

#include "PeerManager.h"

#include "imgui.h"
#include "imgui_internal.h"
#include <string>

namespace Demo
{
    /// Global control panel: peer creation, connection matrix, transport info, stats overview.
    class ControlPanel
    {
    public:
        constexpr static const char* WindowName = "Control Panel";
        
        void Render(PeerManager& mgr)
        {
            // if (auto* w = ImGui::FindWindowByName(WindowName); !w || !w->DockIsActive) {
            //     ImGuiWindowClass wc;
            //     wc.DockNodeFlagsOverrideSet = static_cast<ImGuiDockNodeFlags>(ImGuiDockNodeFlags_NoDockingOverMe | ImGuiDockNodeFlags_NoDockingSplit);
            //     ImGui::SetNextWindowClass(&wc);
            // }
            if (!ImGui::Begin(WindowName)) {
                ImGui::End();
                return;
            }

            RenderPeerCreation(mgr);
            ImGui::Separator();
            RenderConnectionMatrix(mgr);
            ImGui::Separator();
            RenderStats(mgr);

            ImGui::End();
        }

    private:

        void RenderPeerCreation(PeerManager& mgr)
        {
            ImGui::SetNextItemWidth(100);

            char newPeerName[64] = {};
            ImGui::InputTextWithHint("##name", "Name (optional)", newPeerName, sizeof(newPeerName));
            ImGui::SameLine();
            if (ImGui::Button("Create")) {
                mgr.CreatePeer(newPeerName);
            }

            ImGui::SameLine();
            ImGui::Text("(transport: %s)", TransportModeName(mgr.GetTransportMode()));
        }

        void RenderConnectionMatrix(PeerManager& mgr)
        {
            if (!ImGui::TreeNodeEx("Connections", ImGuiTreeNodeFlags_DefaultOpen)) {
                return;
            }

            const auto& entries = mgr.Entries();
            if (entries.empty()) {
                ImGui::TextDisabled("No peers created yet");
                ImGui::TreePop();
                return;
            }

            int n = static_cast<int>(entries.size());
            int pendingRemove = -1;

            if (ImGui::BeginTable("##connmatrix", n + 1,
                    ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit)) {
                ImGui::TableSetupColumn("Peer");
                for (const auto& entry : entries) {
                    ImGui::TableSetupColumn(entry.peer->name.c_str());
                }
                ImGui::TableHeadersRow();

                for (int i = 0; i < n; ++i) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();

                    auto& peer = *entries[i].peer;
                    ImGui::PushID(peer.id);
                    ImGui::ColorButton("##color", peer.color, ImGuiColorEditFlags_NoTooltip, {12, 12});
                    ImGui::SameLine();
                    ImGui::TextUnformatted(peer.name.c_str());
                    ImGui::SameLine();
                    if (ImGui::SmallButton("X")) {
                        pendingRemove = peer.id;
                    }
                    ImGui::PopID();

                    for (int j = 0; j < n; ++j) {
                        ImGui::TableNextColumn();
                        if (i == j) {
                            ImGui::TextDisabled(" - ");
                            continue;
                        }
                        RenderConnectionElement(mgr, i, j);
                    }
                }
                ImGui::EndTable();
            }

            if (pendingRemove != -1) {
                mgr.RemovePeer(pendingRemove);
            }

            ImGui::TreePop();
        }

        void RenderConnectionElement(PeerManager& mgr, int i, int j) const
        {
            const auto& entries = mgr.Entries();
            auto& a = *entries[i].peer;
            auto& b = *entries[j].peer;
            ImGui::PushID(i * entries.size() + j);
            bool connected = mgr.AreConnected(a.id, b.id);
            if (connected) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.2f, 0.65f, 0.3f, 0.8f});
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{0.2f, 0.65f, 0.3f, 1.0f});
                if (ImGui::SmallButton("Dis")) {
                    mgr.Disconnect(a, b);
                }
                ImGui::PopStyleColor(2);
            } else {
                if (ImGui::SmallButton("Con")) {
                    mgr.Connect(a, b);
                }
            }
            ImGui::PopID();
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
