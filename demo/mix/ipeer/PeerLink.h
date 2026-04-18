#pragma once

#include "Peer.h"
#include "Protocol.h"

#include <cstddef>
#include <deque>
#include <random>
#include <vector>

namespace IPeer
{
    /// Connection state between two peers.
    enum class LinkState : std::uint8_t
    {
        Connected, ///< Link established, sync not yet started.
        Syncing,   ///< Probe exchange in progress, not yet converged.
        Synced,    ///< Sync converged, payload flowing.
    };

    /// Per-direction sync and payload state.
    struct DirectionState
    {
        // Sync metrics (computed from probe exchange)
        double rtt = 0.0;
        double offset = 0.0;
        double quality = 0.0; ///< 0.0 = none, 1.0 = perfect.

        // Probe bookkeeping
        int probesSent = 0;
        int probesReceived = 0;
        double lastProbeSendTime = 0.0;

        // Received payload state (last remote position for dead-reckoning)
        PayloadUpdateMsg lastPayload{};
        bool hasPayload = false;

        // Message counters
        int messagesSent = 0;
        int messagesReceived = 0;
    };

    /// Bidirectional connection between two peers.
    /// Uses in-process message queues with simulated latency.
    class PeerLink
    {
    public:
        PeerLink(Peer& peerA, Peer& peerB)
            : _peerA(peerA)
            , _peerB(peerB)
            , _rng(std::random_device{}())
        {}

        [[nodiscard]] Peer& GetPeerA() const { return _peerA; }
        [[nodiscard]] Peer& GetPeerB() const { return _peerB; }
        [[nodiscard]] LinkState GetState() const { return _state; }

        /// Get the direction state from perspective of the given peer.
        [[nodiscard]] const DirectionState& GetDirectionFor(int peerId) const
        {
            return (peerId == _peerA.id) ? _dirAtoB : _dirBtoA;
        }

        /// Get the remote peer relative to the given peer.
        [[nodiscard]] Peer& GetRemotePeer(int localPeerId) const
        {
            return (localPeerId == _peerA.id) ? _peerB : _peerA;
        }

        /// Estimate remote peer position using dead-reckoning extrapolation.
        [[nodiscard]] Vec2 EstimateRemotePosition(int localPeerId, double currentTime) const
        {
            const auto& dir = (localPeerId == _peerA.id) ? _dirAtoB : _dirBtoA;
            if (!dir.hasPayload) {
                return {};
            }
            auto dt = static_cast<float>(currentTime - dir.lastPayload.timestamp);
            return {
                .x = dir.lastPayload.x + dir.lastPayload.vx * dt,
                .y = dir.lastPayload.y + dir.lastPayload.vy * dt,
            };
        }

        /// Process one frame: exchange probes and payloads on schedule.
        void ProcessFrame(float dt, double sessionTime)
        {
            _elapsedSinceProbe += dt;
            _elapsedSincePayload += dt;

            // Deliver queued messages (simulate transport delay)
            DeliverMessages(sessionTime);

            // Probe exchange (~10 probes/sec during syncing, ~2/sec when synced)
            float probeInterval = (_state == LinkState::Synced) ? 0.5f : 0.1f;
            if (_elapsedSinceProbe >= probeInterval) {
                _elapsedSinceProbe = 0.0f;
                SendProbe(_peerA.id, sessionTime);
                SendProbe(_peerB.id, sessionTime);
            }

            // Payload exchange (~20/sec)
            if (_elapsedSincePayload >= 0.05f) {
                _elapsedSincePayload = 0.0f;
                SendPayloadUpdate(_peerA.id, sessionTime);
                SendPayloadUpdate(_peerB.id, sessionTime);
            }

            UpdateState();
        }

    private:
        Peer& _peerA;
        Peer& _peerB;
        LinkState _state = LinkState::Connected;

        DirectionState _dirAtoB; ///< A's view of B (metrics for messages A sends to B).
        DirectionState _dirBtoA; ///< B's view of A.

        /// Queued messages with simulated delivery time.
        struct QueuedMessage
        {
            int fromPeerId;
            std::vector<std::byte> data;
            double deliverAt; ///< Session time when message should be delivered.
        };
        std::deque<QueuedMessage> _queue;

        float _elapsedSinceProbe = 0.0f;
        float _elapsedSincePayload = 0.0f;
        std::mt19937 _rng;

        /// Simulated one-way latency: 5-15ms.
        double SimulatedLatency()
        {
            std::uniform_real_distribution<double> dist(0.005, 0.015);
            return dist(_rng);
        }

        void Enqueue(int fromPeerId, std::vector<std::byte> data, double sessionTime)
        {
            _queue.push_back({
                .fromPeerId = fromPeerId,
                .data = std::move(data),
                .deliverAt = sessionTime + SimulatedLatency(),
            });
        }

        DirectionState& SenderDirection(int fromPeerId)
        {
            return (fromPeerId == _peerA.id) ? _dirAtoB : _dirBtoA;
        }

        DirectionState& ReceiverDirection(int fromPeerId)
        {
            return (fromPeerId == _peerA.id) ? _dirBtoA : _dirAtoB;
        }

        void SendProbe(int fromPeerId, double sessionTime)
        {
            auto& dir = SenderDirection(fromPeerId);
            auto msg = SerializeSyncProbeReq({.t1 = sessionTime});
            dir.probesSent++;
            dir.messagesSent++;
            dir.lastProbeSendTime = sessionTime;
            Enqueue(fromPeerId, std::move(msg), sessionTime);
        }

        void SendPayloadUpdate(int fromPeerId, double sessionTime)
        {
            Peer& peer = (fromPeerId == _peerA.id) ? _peerA : _peerB;
            auto& dir = SenderDirection(fromPeerId);
            auto msg = SerializePayloadUpdate({
                .x = peer.position.x,
                .y = peer.position.y,
                .vx = peer.velocity.x,
                .vy = peer.velocity.y,
                .timestamp = sessionTime,
            });
            dir.messagesSent++;
            Enqueue(fromPeerId, std::move(msg), sessionTime);
        }

        void DeliverMessages(double sessionTime)
        {
            while (!_queue.empty() && _queue.front().deliverAt <= sessionTime) {
                auto& queued = _queue.front();
                HandleMessage(queued.fromPeerId, queued.data, sessionTime);
                _queue.pop_front();
            }
        }

        void HandleMessage(int fromPeerId, std::span<const std::byte> data, double sessionTime)
        {
            auto parsed = ParseMessage(data);
            if (!parsed) {
                return;
            }

            auto& recvDir = ReceiverDirection(fromPeerId);
            recvDir.messagesReceived++;

            switch (parsed->type) {
                case MsgType::SyncProbeReq: {
                    auto req = ReadPayload<SyncProbeReqMsg>(parsed->payload);
                    if (!req) {
                        break;
                    }
                    // Respond immediately (response goes back to sender)
                    auto resp = SerializeSyncProbeResp({
                        .t1 = req->t1,
                        .t2 = sessionTime,
                        .t3 = sessionTime,
                    });
                    int responderId = (fromPeerId == _peerA.id) ? _peerB.id : _peerA.id;
                    SenderDirection(responderId).messagesSent++;
                    Enqueue(responderId, std::move(resp), sessionTime);
                    break;
                }
                case MsgType::SyncProbeResp: {
                    auto resp = ReadPayload<SyncProbeRespMsg>(parsed->payload);
                    if (!resp) {
                        break;
                    }
                    // Compute RTT and offset (NTP algorithm)
                    double t4 = sessionTime;
                    double roundTrip = (t4 - resp->t1) - (resp->t3 - resp->t2);
                    double clockOffset = ((resp->t2 - resp->t1) + (resp->t3 - t4)) / 2.0;

                    // Exponential moving average
                    auto& dir = ReceiverDirection(fromPeerId);
                    constexpr double alpha = 0.3;
                    dir.rtt = dir.rtt * (1.0 - alpha) + roundTrip * alpha;
                    dir.offset = dir.offset * (1.0 - alpha) + clockOffset * alpha;
                    dir.probesReceived++;

                    // Quality ramps up with successful probes (converges around 5-10 probes)
                    double convergence = std::min(1.0, static_cast<double>(dir.probesReceived) / 8.0);
                    dir.quality = convergence * std::max(0.0, 1.0 - dir.rtt * 20.0);
                    break;
                }
                case MsgType::PayloadUpdate: {
                    auto update = ReadPayload<PayloadUpdateMsg>(parsed->payload);
                    if (!update) {
                        break;
                    }
                    recvDir.lastPayload = *update;
                    recvDir.hasPayload = true;
                    break;
                }
            }
        }

        void UpdateState()
        {
            bool aSyncing = _dirAtoB.probesReceived > 0;
            bool bSyncing = _dirBtoA.probesReceived > 0;
            bool aSynced = _dirAtoB.quality > 0.5;
            bool bSynced = _dirBtoA.quality > 0.5;

            if (aSynced && bSynced) {
                _state = LinkState::Synced;
            } else if (aSyncing || bSyncing) {
                _state = LinkState::Syncing;
            } else {
                _state = LinkState::Connected;
            }
        }
    };
}
