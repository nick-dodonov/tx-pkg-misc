#pragma once

#include "PeerManager.h"

#include "imgui.h"
#include "SynTm/Types.h"
#include <chrono>
#include <cmath>

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

            // epochOffset = SyncedNow - LocalNow. Unique per peer; reflects clock
            // origin difference from epoch owner. Zero for epoch owner.
            double syncOffsetMs =
                static_cast<double>((syncNow - localNow).count()) / 1'000'000.0;
            ImGui::Text("sync offset: %+.2f ms", syncOffsetMs);
        }

        void RenderConnections(PeerNode* node) const
        {
            if (!node || node->Links().empty()) {
                ImGui::TextDisabled("No connections");
                return;
            }

            constexpr ImGuiTableFlags kTableFlags =
                ImGuiTableFlags_RowBg |
                ImGuiTableFlags_Borders;
            if (ImGui::BeginTable("##conns", 6, kTableFlags)) {
                ImGui::TableSetupColumn("Peer");
                ImGui::TableSetupColumn("Connected");
                ImGui::TableSetupColumn("Has Payload");
                ImGui::TableSetupColumn("Sync Quality");
                ImGui::TableSetupColumn("Role");
                ImGui::TableSetupColumn("Epoch Src");
                ImGui::TableHeadersRow();

                auto epochSrcId = _peer.consensus.EpochSourcePeerId();

                for (const auto& [remotePeerId, ls] : node->Links()) {
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
                    auto* session = _peer.consensus.GetSession(remotePeerId);
                    if (session) {
                        auto q = session->Quality();
                        ImGui::Text("%s", SynTm::SyncQualityToString(q).data());
                    } else {
                        ImGui::TextDisabled("N/A");
                    }

                    ImGui::TableNextColumn();
                    bool isActive = SynTm::Consensus::IsActivePeer(_peer.peerId, remotePeerId);
                    ImGui::TextColored(
                        isActive ? ColRoleActive : ColRolePassive,
                        "%s", isActive ? "Active" : "Passive");

                    ImGui::TableNextColumn();
                    bool isEpochSrc = (epochSrcId == remotePeerId);
                    if (isEpochSrc) {
                        ImGui::TextColored(ColEpochSrc, "yes");
                    } else {
                        ImGui::TextDisabled("no");
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
            bool isOwner = _peer.consensus.IsEpochOwner();

            ImGui::Text("Synced: %s", synced ? "yes" : "no");
            ImGui::Text("Quality: %s", SynTm::SyncQualityToString(quality).data());
            ImGui::Text("Epoch owner: %s", isOwner ? "yes" : "no");

            // Show epoch source peer (the peer we learned the current epoch from).
            auto epochSrcId = _peer.consensus.EpochSourcePeerId();
            if (isOwner || epochSrcId.empty()) {
                ImGui::Text("Epoch source: (self)");
            } else {
                ImGui::Text("Epoch source: %.*s",
                    static_cast<int>(epochSrcId.size()), epochSrcId.data());
            }

            ImGui::Text("Peer count: %zu", _peer.consensus.PeerCount());

            const auto& epoch = _peer.consensus.Epoch();
            if (epoch.IsValid()) {
                ImGui::Text("Epoch ID: %llu", static_cast<unsigned long long>(epoch.id));
                ImGui::Text("Epoch members: %u", epoch.memberCount);
            } else {
                ImGui::TextDisabled("No epoch");
            }

            // Per-link sync diagnostics table.
            if (_peer.consensus.PeerCount() > 0) {
                ImGui::Separator();
                ImGui::TextUnformatted("Per-link diagnostics:");

                constexpr ImGuiTableFlags kTableFlags =
                    ImGuiTableFlags_RowBg |
                    ImGuiTableFlags_Borders |
                    ImGuiTableFlags_SizingFixedFit;

                if (ImGui::BeginTable("##syncdiag", 7, kTableFlags)) {
                    ImGui::TableSetupColumn("Peer");
                    ImGui::TableSetupColumn("RTT min");
                    ImGui::TableSetupColumn("RTT max");
                    ImGui::TableSetupColumn("RTT mean");
                    ImGui::TableSetupColumn("Off. mean");
                    ImGui::TableSetupColumn("Samples");
                    ImGui::TableSetupColumn("Steps");
                    ImGui::TableHeadersRow();

                    _peer.consensus.ForEachPeer([&](const std::string& peerId) {
                        auto diagOpt = _peer.consensus.GetSessionDiagnostics(peerId);
                        if (!diagOpt) {
                            return;
                        }
                        const auto& d = *diagOpt;

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(peerId.c_str());

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

                        ImGui::TableNextColumn();
                        ImGui::Text("%zu", d.sampleCount);

                        ImGui::TableNextColumn();
                        ImGui::Text("%u", d.stepCount);
                    });

                    ImGui::EndTable();
                }

                // epochOffset = SyncedNow - LocalNow: informational, unique per peer.
                // Each peer has an independent clock origin, so this value is NOT
                // comparable across peers. Use ControlPanel "Δ epoch" to compare.
                // RemoteNow is the remote peer's raw steady time — also not
                // comparable to local time (independent clock origins).
                ImGui::Separator();

                auto localNow = _peer.clock.Now();
                auto syncNow = _peer.syncClock.Now();
                double syncOffsetMs =
                    static_cast<double>((syncNow - localNow).count()) / 1'000'000.0;
                ImGui::Text("epochOffset (synced-local): %+.2f ms", syncOffsetMs);

                ImGui::TextUnformatted("Session RemoteNow (raw remote local time):");
                _peer.consensus.ForEachPeer([&](const std::string& peerId) {
                    const auto* session = _peer.consensus.GetSession(peerId);
                    if (!session) {
                        return;
                    }
                    // RemoteNow is expressed in the remote peer's own steady clock.
                    // It cannot be compared to LocalNow — difference is always large.
                    SynTm::Ticks remoteNow = session->RemoteNow();
                    double remoteSeconds =
                        std::chrono::duration<double>(remoteNow).count();
                    ImGui::Text("  %s: %.3f s", peerId.c_str(), remoteSeconds);
                });
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
