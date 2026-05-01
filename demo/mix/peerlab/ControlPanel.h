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
        // Delta epoch sync quality colors
        static constexpr ImVec4 ColDeltaGood   = {0.26f, 0.85f, 0.42f, 1.0f}; // green  — < 5 ms
        static constexpr ImVec4 ColDeltaWarn   = {0.95f, 0.75f, 0.20f, 1.0f}; // yellow — < 20 ms
        static constexpr ImVec4 ColDeltaBad    = {0.98f, 0.39f, 0.26f, 1.0f}; // red    — >= 20 ms

        // Connected button colors
        static constexpr ImVec4 ColConnected        = {0.2f, 0.65f, 0.3f, 0.8f}; // green
        static constexpr ImVec4 ColConnectedHovered = {0.2f, 0.65f, 0.3f, 1.0f}; // green (hovered)

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

            auto columnsCount = 1 // Row header
                + n // Peer columns
                + 1 // Local clock column
                + 1 // Synced clock column
                + 1 // Δ epoch column (syncNow - epochOwner.localNow)
                ;
            constexpr auto tableFlags = 
                ImGuiTableFlags_Borders |
                ImGuiTableFlags_SizingFixedFit;
            if (ImGui::BeginTable("##connmatrix", columnsCount, tableFlags)) {
                RenderConnectionHeaders(mgr);

                for (int i = 0; i < n; ++i) {
                    ImGui::TableNextRow();
                    RenderConnectionRow(mgr, i, n, pendingRemove);
                }
                ImGui::EndTable();
            }

            if (pendingRemove != -1) {
                mgr.RemovePeer(pendingRemove);
            }

            ImGui::TreePop();
        }

        void RenderConnectionHeaders(PeerManager& mgr)
        {
            ImGui::TableSetupColumn("");

            const auto& entries = mgr.Entries();
            for (const auto& entry : entries) {
                ImGui::TableSetupColumn(entry.peer->name.c_str());
            }

            ImGui::TableSetupColumn("local");
            ImGui::TableSetupColumn("synced");
            ImGui::TableSetupColumn("\xce\x94 epoch"); // Δ epoch

            ImGui::TableHeadersRow();
        }

        void RenderConnectionRow(PeerManager& mgr, int i, int n, int& pendingRemove)
        {
            ImGui::TableNextColumn();

            const auto& entries = mgr.Entries();
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

            // local time
            ImGui::TableNextColumn();
            auto localNow = peer.clock.Now();
            double localSeconds = std::chrono::duration<double>(localNow).count();
            ImGui::Text("%.6f s", localSeconds);

            // synced time
            ImGui::TableNextColumn();
            auto syncNow = peer.syncClock.Now();
            double syncSeconds = std::chrono::duration<double>(syncNow).count();
            ImGui::Text("%.6f s", syncSeconds);

            // Δ epoch: syncNow - epochOwner.localNow, where the owner is the peer
            // that owns the same epoch as this peer (matched by epoch ID).
            // Multiple disconnected groups may coexist, each with its own owner.
            ImGui::TableNextColumn();
            const auto thisEpochId = peer.consensus.Epoch().id;
            const Peer* epochOwner = nullptr;
            for (const auto& e : entries) {
                if (e.peer->consensus.IsEpochOwner() &&
                    e.peer->consensus.Epoch().id == thisEpochId) {
                    epochOwner = e.peer.get();
                    break;
                }
            }
            if (epochOwner) {
                const auto epochOwnerLocalNow = epochOwner->clock.Now();
                double deltaMs =
                    static_cast<double>((syncNow - epochOwnerLocalNow).count()) / 1'000'000.0;
                double absDelta = std::abs(deltaMs);
                ImVec4 col = absDelta < 5.0
                    ? ColDeltaGood
                    : absDelta < 20.0
                        ? ColDeltaWarn
                        : ColDeltaBad;
                ImGui::TextColored(col, "%+.2f ms", deltaMs);
            } else {
                ImGui::TextDisabled("-");
            }
        }

        void RenderConnectionElement(PeerManager& mgr, int i, int j) const
        {
            const auto& entries = mgr.Entries();
            auto& initiator = *entries[i].peer;
            auto& responder = *entries[j].peer;
            ImGui::PushID(i * entries.size() + j);
            bool connected = mgr.AreConnected(initiator.id, responder.id);
            if (connected) {
                ImGui::PushStyleColor(ImGuiCol_Button, ColConnected);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ColConnectedHovered);
                if (ImGui::SmallButton("Dis")) {
                    mgr.Disconnect(initiator, responder);
                }
                ImGui::PopStyleColor(2);
            } else {
                if (ImGui::SmallButton("Con")) {
                    mgr.Connect(initiator, responder);
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
