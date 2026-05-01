#pragma once

#include "PeerManager.h"
#include "Im/Ext.h"

#include "imgui.h"
#include <string>

namespace Demo
{
    /// Global control panel: peer creation, connection matrix, transport info, stats overview.
    class ControlPanel
    {
    public:
        constexpr static auto WindowName = "Control Panel";

        explicit ControlPanel(PeerManager& mgr)
            : _mgr(mgr)
        {}

        void Render() const
        {
            if (!ImGui::Begin(WindowName)) {
                ImGui::End();
                return;
            }

            RenderConnectionMatrix();
            ImGui::Separator();
            RenderStats();

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

        PeerManager& _mgr;

        void RenderPeerCreation() const
        {
            if (ImGui::Button("Create")) {
                _mgr.CreatePeer();
            }

            ImGui::SameLine();
            ImGui::Text("(transport: %s)", TransportModeName(_mgr.GetTransportMode()));
        }

        void RenderConnectionMatrix() const
        {
            if (!ImGui::TreeNodeEx("Connections", ImGuiTreeNodeFlags_DefaultOpen)) {
                return;
            }
            RenderPeerCreation();

            const auto& entries = _mgr.Entries();
            if (entries.empty()) {
                ImGui::TextDisabled("No peers created yet");
                ImGui::TreePop();
                return;
            }

            const auto n = static_cast<int>(entries.size());
            auto pendingRemove = -1;

            const auto columnsCount = 1 // Row header
                + n // Peer columns
                + 1 // Local clock column
                + 1 // Synced clock column
                + 1 // Δ epoch column (syncNow - epochOwner.localNow)
                ;
            constexpr auto tableFlags = 
                ImGuiTableFlags_Borders |
                ImGuiTableFlags_SizingFixedFit;
            if (ImGui::BeginTable("##conn-matrix", columnsCount, tableFlags)) {
                RenderConnectionHeaders();

                for (auto i = 0; i < n; ++i) {
                    ImGui::TableNextRow();
                    RenderConnectionRow(i, n, pendingRemove);
                }
                ImGui::EndTable();
            }

            if (pendingRemove != -1) {
                _mgr.RemovePeer(pendingRemove);
            }

            ImGui::TreePop();
        }

        void RenderConnectionHeaders() const
        {
            ImGui::TableSetupColumn("");

            const auto& entries = _mgr.Entries();
            for (const auto& entry : entries) {
                ImGui::TableSetupColumn(entry.peer->peerId.c_str());
            }

            ImGui::TableSetupColumn("local");
            ImGui::TableSetupColumn("synced");
            ImGui::TableSetupColumn("\xce\x94 epoch"); // Δ epoch

            ImGui::TableHeadersRow();
        }

        void RenderConnectionRow(const int i, const int n, int& pendingRemove) const
        {
            ImGui::TableNextColumn();

            const auto& entries = _mgr.Entries();
            const auto& peer = *entries[i].peer;
            ImGui::PushID(peer.id);
            ImGui::ColorButton("##color", peer.color, ImGuiColorEditFlags_NoTooltip, {12, 12});
            ImGui::SameLine();
            ImGui::TextUnformatted(peer.peerId.c_str());
            ImGui::SameLine();
            if (ImGui::SmallButton("X")) {
                pendingRemove = peer.id;
            }
            ImGui::PopID();

            for (auto j = 0; j < n; ++j) {
                ImGui::TableNextColumn();
                if (i == j) {
                    ImGui::TextDisabled(" - ");
                    continue;
                }
                RenderConnectionElement(i, j);
            }

            const auto localNow = peer.LocalNow();
            const auto syncNow = peer.SyncedNow();

            // local time
            ImGui::TableNextColumn();
            const auto localSeconds = std::chrono::duration<double>(localNow).count();
            ImGui::Text("%.6f s", localSeconds);

            // synced time
            ImGui::TableNextColumn();
            const auto syncSeconds = std::chrono::duration<double>(syncNow).count();
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

            Im::PushDefaultMonoFont();
            if (epochOwner) {
                const auto epochOwnerLocalNow = epochOwner->LocalNow();
                const auto deltaMs = static_cast<double>((syncNow - epochOwnerLocalNow).count()) / 1'000'000.0;
                const auto absDelta = std::abs(deltaMs);
                const auto col = absDelta < 1.0
                    ? ColDeltaGood
                    : absDelta < 5.0 //NOLINT(readability-avoid-nested-conditional-operator)
                        ? ColDeltaWarn
                        : ColDeltaBad;
                ImGui::TextColored(col, "%+.3f ms", deltaMs);
            } else {
                ImGui::TextDisabled("-");
            }
            Im::PopDefaultMonoFont();
        }

        void RenderConnectionElement(const int i, const int j) const
        {
            const auto& entries = _mgr.Entries();
            auto& initiator = *entries[i].peer;
            auto& responder = *entries[j].peer;
            ImGui::PushID(i * static_cast<int>(entries.size()) + j);
            if (_mgr.AreConnected(initiator.id, responder.id)) {
                ImGui::PushStyleColor(ImGuiCol_Button, ColConnected);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ColConnectedHovered);
                if (ImGui::SmallButton("D")) { // Disconnect
                    _mgr.Disconnect(initiator, responder);
                }
                ImGui::PopStyleColor(2);
            } else {
                if (ImGui::SmallButton("C")) { // Connect
                    _mgr.Connect(initiator, responder);
                }
            }
            ImGui::PopID();
        }

        void RenderStats() const
        {
            if (!ImGui::TreeNodeEx("Stats", ImGuiTreeNodeFlags_DefaultOpen)) {
                return;
            }

            ImGui::Text("Peers: %zu", _mgr.Entries().size());

            // Count total links and synced peers.
            auto totalLinks = 0;
            auto syncedPeers = 0;
            for (const auto& entry : _mgr.Entries()) {
                totalLinks += static_cast<int>(entry.node->Links().size());
                if (entry.peer->consensus.IsSynced()) {
                    syncedPeers++;
                }
            }
            ImGui::Text("Links: %d", totalLinks / 2); // Each link counted twice.
            ImGui::Text("Synced peers: %d / %zu", syncedPeers, _mgr.Entries().size());

            ImGui::TreePop();
        }
    };
}
