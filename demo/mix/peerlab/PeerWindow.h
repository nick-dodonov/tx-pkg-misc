#pragma once

#include "PeerManager.h"
#include "UiHelpers.h"

#include "SynTm/Types.h"

#include "imgui.h"
#include <chrono>
#include <format>

namespace Demo
{
    /// Per-peer ImGui window: canvas, connection table, sync diagnostics, controls.
    class PeerWindow
    {
    public:
        explicit PeerWindow(Peer& peer) : _peer(peer) {}

        [[nodiscard]] const Peer& GetPeer() const { return _peer; }
        [[nodiscard]] int GetPeerId() const { return _peer.id; }

        void Render(const PeerManager& mgr)
        {
            if (_firstRender) {
                ImGui::SetNextWindowFocus();
                _firstRender = false;
            }
            auto open = true;
            if (!ImGui::Begin(_peer.peerId.c_str(), &open)) {
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
            RenderLinksDiagnostics(node);

            ImGui::Separator();
            RenderSyncDiagnostics(mgr);

            ImGui::Separator();
            RenderControls(mgr, node);

            ImGui::End();
        }

        [[nodiscard]] bool WantClose() const { return _wantClose; }

    private:
        // Connection status colors
        static constexpr ImVec4 ColLinkYes  = {0.26f, 0.85f, 0.42f, 1.0f}; // green  — connected
        static constexpr ImVec4 ColLinkNo   = {0.98f, 0.39f, 0.26f, 1.0f}; // red    — disconnected

        // Peer role colors
        static constexpr ImVec4 ColRoleActive  = {0.40f, 0.60f, 1.0f, 1.0f}; // blue   — active peer
        static constexpr ImVec4 ColRolePassive = {0.75f, 0.45f, 1.0f, 1.0f}; // purple — passive peer

        // Epoch source highlight color
        static constexpr ImVec4 ColEpochSrc = {0.95f, 0.75f, 0.20f, 1.0f}; // yellow — epoch source

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
            constexpr ImVec2 canvasSize{100, 100};
            auto canvasMin = ImGui::GetCursorScreenPos();

            auto* drawList = ImGui::GetWindowDrawList();
            ImVec2 canvasMax{canvasMin.x + canvasSize.x, canvasMin.y + canvasSize.y};
            drawList->AddRectFilled(canvasMin, canvasMax, IM_COL32(20, 20, 40, 255));
            drawList->AddRect(canvasMin, canvasMax, IM_COL32(80, 80, 100, 255));

            // Grid
            for (auto i = 1; i < 4; ++i) {
                auto x = canvasMin.x + canvasSize.x * (static_cast<float>(i) / 4.0f);
                auto y = canvasMin.y + canvasSize.y * (static_cast<float>(i) / 4.0f);
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
                    const Vec2 remotePos{
                        .x = ls.lastPayload.x,// + ls.lastPayload.vx * dt,
                        .y = ls.lastPayload.y,// + ls.lastPayload.vy * dt,
                    };
                    auto px = ToCanvas(remotePos, canvasMin, canvasSize);
                    drawList->AddCircleFilled(px, 5.0f, IM_COL32(200, 200, 200, 200));
                    drawList->AddText({px.x + 7, px.y - 6}, IM_COL32(200, 200, 200, 200),
                        remotePeerId.c_str());
                }
            }

            // Draw local peer position (larger, on top).
            {
                const auto px = ToCanvas(_peer.position, canvasMin, canvasSize);
                const auto col = ImGui::ColorConvertFloat4ToU32(_peer.color);
                drawList->AddCircleFilled(px, 8.0f, col);
                drawList->AddCircle(px, 8.0f, IM_COL32(255, 255, 255, 180), 0, 1.5f);
            }

            ImGui::Dummy(canvasSize);
        }

        void RenderPeerState() const
        {
            const auto localNow = _peer.LocalNow();
            const auto syncNow = _peer.SyncedNow();

            ImGui::Text("id: %s", _peer.peerId.c_str());
            ImGui::Text("position: (%.3f, %.3f)", _peer.position.x, _peer.position.y);
            ImGui::Text("velocity: (%.3f, %.3f)", _peer.velocity.x, _peer.velocity.y);

            const auto localSeconds = std::chrono::duration<double>(localNow).count();
            ImGui::Text("local time: %.3f s", localSeconds);

            const auto syncSeconds = std::chrono::duration<double>(syncNow).count();
            ImGui::Text("sync time: %.3f s", syncSeconds);

            // syncOffsetMs = SyncedNow - LocalNow: informational, unique per peer.
            // Each peer has an independent clock origin, so this value is NOT comparable across peers.
            const auto syncOffsetMs = static_cast<double>((syncNow - localNow).count()) / 1'000'000.0;
            ImGui::Text("sync offset: %+.3f ms", syncOffsetMs);
        }

        void RenderLinksDiagnostics(const PeerNode* node) const
        {
            if (!ImGui::TreeNodeEx("Links Diagnostics", ImGuiTreeNodeFlags_DefaultOpen)) {
                return;
            }

            if (!node || node->Links().empty()) {
                ImGui::TextDisabled("No connections");
                ImGui::TreePop();
                return;
            }

            constexpr auto kTableFlags =
                ImGuiTableFlags_RowBg |
                ImGuiTableFlags_Borders |
                ImGuiTableFlags_SizingFixedFit;
            if (ImGui::BeginTable("##links", 4, kTableFlags)) {
                ImGui::TableSetupColumn("Peer");
                ImGui::TableSetupColumn("Link");
                ImGui::TableSetupColumn("Payload");
                ImGui::TableSetupColumn("Role");
                ImGui::TableHeadersRow();

                for (const auto& [remotePeerId, ls] : node->Links()) {
                    RenderLinkRow(remotePeerId, ls);
                }
                ImGui::EndTable();
            }

            ImGui::TreePop();
        }

        void RenderLinkRow(const auto& remotePeerId, const auto& ls) const
        {
            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            ImGui::Text("%s", remotePeerId.c_str());

            ImGui::TableNextColumn();
            ImGui::TextColored(
                ls.link ? ColLinkYes : ColLinkNo,
                "%s", ls.link ? "yes" : "no");

            ImGui::TableNextColumn();
            ImGui::Text("%s", ls.hasPayload ? "yes" : "no");

            ImGui::TableNextColumn();
            const auto isActive = SynTm::Consensus::IsActivePeer(_peer.peerId, remotePeerId);
            ImGui::TextColored(
                isActive ? ColRoleActive : ColRolePassive,
                "%s", isActive ? "Active" : "Passive");
        }

        void RenderConsensusTable() const {
            constexpr auto kTableFlags =
                ImGuiTableFlags_RowBg |
                ImGuiTableFlags_Borders |
                ImGuiTableFlags_SizingFixedFit;
            if (ImGui::BeginTable("##links-diag", 8, kTableFlags)) {
                ImGui::TableSetupColumn("Peer");
                ImGui::TableSetupColumn("Count");
                ImGui::TableSetupColumn("Steps");
                ImGui::TableSetupColumn("Epoch");
                ImGui::TableSetupColumn("RTT min");
                ImGui::TableSetupColumn("RTT max");
                ImGui::TableSetupColumn("RTT mean");
                ImGui::TableSetupColumn("Off. mean");
                ImGui::TableHeadersRow();

                const auto epochSrcId = _peer.consensus.EpochSourcePeerId();

                _peer.consensus.ForEachPeerId([&](const std::string& peerId) {
                    const auto diagOpt = _peer.consensus.GetSessionDiagnostics(peerId);
                    if (!diagOpt) {
                        return;
                    }
                    const auto& d = *diagOpt;

                    ImGui::TableNextRow();

                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(peerId.c_str());

                    ImGui::TableNextColumn();
                    ImGui::Text("%zu", d.sampleCount);

                    ImGui::TableNextColumn();
                    ImGui::Text("%u", d.stepCount);

                    ImGui::TableNextColumn();
                    if (peerId == epochSrcId) {
                        ImGui::TextColored(ColEpochSrc, "yes");
                    } else {
                        ImGui::TextDisabled("no");
                    }

                    // Convert nanoseconds to milliseconds for display.
                    auto toMs = [](SynTm::Ticks t) {
                        return static_cast<double>(t.count()) / 1'000'000.0;
                    };

                    ImGui::TableNextColumn();
                    if (d.sampleCount > 0) {
                        ImGui::Text("%.2f ms", toMs(d.rttMin));
                    } else {
                        ImGui::TextDisabled("-");
                    }

                    ImGui::TableNextColumn();
                    if (d.sampleCount > 0) {
                        ImGui::Text("%.2f ms", toMs(d.rttMax));
                    } else {
                        ImGui::TextDisabled("-");
                    }

                    ImGui::TableNextColumn();
                    if (d.sampleCount > 0) {
                        ImGui::Text("%.2f ms", toMs(d.rttMean));
                    } else {
                        ImGui::TextDisabled("-");
                    }

                    ImGui::TableNextColumn();
                    if (d.sampleCount > 0) {
                        ImGui::Text("%+.2f ms", toMs(d.offsetMean));
                    } else {
                        ImGui::TextDisabled("-");
                    }
                });

                ImGui::EndTable();
            }
        }

        void RenderSyncDiagnostics(const PeerManager& mgr) const
        {
            if (!ImGui::TreeNodeEx("Sync Diagnostics", ImGuiTreeNodeFlags_DefaultOpen)) {
                return;
            }

            const auto peerCount = _peer.consensus.PeerCount();
            const auto synced = _peer.consensus.IsSynced();
            const auto quality = _peer.consensus.Quality();
            const auto isOwner = _peer.consensus.IsEpochOwner();

            ImGui::Text("Peer count: %zu", peerCount);
            ImGui::Text("Synced: %s", synced ? "yes" : "no");
            ImGui::Text("Quality: %s", std::format("{}", SynTm::SyncQualityToString(quality)).c_str());
            ImGui::Text("Epoch owner: %s", isOwner ? "yes" : "no");

            // Show epoch source peer (the peer we learned the current epoch from).
            const auto epochSrcId = _peer.consensus.EpochSourcePeerId();
            if (isOwner || epochSrcId.empty()) {
                ImGui::Text("Epoch source: (self)");
            } else {
                ImGui::Text("Epoch source: %.*s",
                    static_cast<int>(epochSrcId.size()), epochSrcId.data());
            }

            const auto& epoch = _peer.consensus.Epoch();
            if (epoch.IsValid()) {
                ImGui::Text("Epoch ID: %llu", static_cast<unsigned long long>(epoch.id));
                ImGui::Text("Epoch members: %u", epoch.memberCount);
            } else {
                ImGui::TextDisabled("No epoch");
            }

            // Per-link sync diagnostics table.
            if (peerCount > 0) {
                ImGui::Separator();
                RenderConsensusTable();

                ImGui::Separator();
                ImGui::TextUnformatted("Sessions:");
                constexpr auto kTableFlags =
                    ImGuiTableFlags_RowBg |
                    ImGuiTableFlags_Borders |
                    ImGuiTableFlags_SizingFixedFit;
                if (ImGui::BeginTable("##sessions-diag", 3, kTableFlags)) {
                    ImGui::TableSetupColumn("peer");
                    ImGui::TableSetupColumn("remote");
                    ImGui::TableSetupColumn("Δ local");
                    ImGui::TableHeadersRow();

                    _peer.consensus.ForEachPeerId([&](const std::string& peerId) {
                        const auto* session = _peer.consensus.GetSession(peerId);
                        if (!session) {
                            return;
                        }
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(peerId.c_str());

                        // RemoteNow is the remote peer's raw steady time — not comparable to my local time (independent clock origins)
                        //  but comparable with peer's local time (only when both peers in the same process).
                        const auto* peer = mgr.FindPeer(peerId);
                        const auto peerLocalNow = peer ? peer->LocalNow() : SynTm::Ticks{};

                        const auto remoteNow = session->RemoteNow();

                        ImGui::TableNextColumn();
                        const auto remoteSeconds = std::chrono::duration<double>(remoteNow).count();
                        ImGui::Text("%.3f s", remoteSeconds);

                        ImGui::TableNextColumn();
                        if (peer) {
                            const auto deltaMs = static_cast<double>((remoteNow - peerLocalNow).count()) / 1'000'000.0;
                            const auto col = GetDeltaCol(deltaMs, 1.0, 5.0);
                            ImGui::TextColored(col, "%+.3f ms", deltaMs);
                        } else {
                            ImGui::TextDisabled("-");
                        }
                    });

                    ImGui::EndTable();
                }
            }

            ImGui::TreePop();
        }

        void RenderControls(const PeerManager& mgr, PeerNode* node) const
        {
            if (!ImGui::TreeNodeEx("Controls", ImGuiTreeNodeFlags_DefaultOpen)) {
                return;
            }

            const auto& entries = mgr.Entries();

            // Connect buttons for not existing links.
            for (const auto& entry : entries) {
                if (entry.peer->id == _peer.id || mgr.AreConnected(_peer.id, entry.peer->id)) {
                    continue;
                }
                ImGui::PushID(entry.peer->id);
                char label[64];
                std::snprintf(label, sizeof(label), "Connect to %s", entry.peer->peerId.c_str());
                if (ImGui::SmallButton(label)) {
                    mgr.Connect(_peer, *entry.peer);
                }
                ImGui::PopID();
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
