#pragma once

#include "Peer.h"
#include "Protocol.h"

#include "Exec/RunTask.h"
#include "Log/Log.h"
#include "Rtt/Acceptor.h"
#include "Rtt/Handler.h"
#include "Rtt/Link.h"
#include "Rtt/Transport.h"
#include "SynTm/Integrate.h"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <exec/timed_scheduler.hpp>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <stdexec/execution.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace Demo
{
    /// Per-link bridge: transport callbacks enqueue messages for the coroutine.
    struct LinkBridge : std::enable_shared_from_this<LinkBridge>
    {
        /// A received message: data payload (nullopt = disconnect signal)
        /// plus the local clock time at which the bytes arrived on the transport.
        struct TimestampedMsg
        {
            std::optional<std::vector<std::byte>> data;
            SynTm::Ticks receivedAt{};
        };

        std::mutex mutex;
        std::queue<TimestampedMsg> pending;

        /// Create a link handler that timestamps every incoming message.
        /// @param clock  The peer's local clock (read from transport thread — safe
        ///               because AppClock::Now() is const and reads steady_clock).
        [[nodiscard]] Rtt::LinkHandler MakeHandler(SynTm::IClock* clock)
        {
            std::weak_ptr<LinkBridge> self = weak_from_this();
            return {
                .onReceived = [self, clock](std::span<const std::byte> data) {
                    if (auto bridge = self.lock()) {
                        const SynTm::Ticks ts = clock ? clock->Now() : SynTm::Ticks{};
                        std::scoped_lock lock(bridge->mutex);
                        bridge->pending.push({
                            .data = std::vector<std::byte>{data.begin(), data.end()},
                            .receivedAt = ts,
                        });
                    }
                },
                .onDisconnected = [self]() {
                    if (auto bridge = self.lock()) {
                        std::scoped_lock lock(bridge->mutex);
                        bridge->pending.push({.data = std::nullopt});
                    }
                },
            };
        }

        [[nodiscard]] std::queue<TimestampedMsg> Drain()
        {
            std::scoped_lock lock(mutex);
            std::queue<TimestampedMsg> result;
            std::swap(result, pending);
            return result;
        }
    };

    /// Per-link state: an accepted link with its bridge and metadata.
    struct PeerLinkState
    {
        std::string remotePeerId;
        std::shared_ptr<Rtt::ILink> link;
        std::shared_ptr<LinkBridge> bridge;

        // Remote peer view (dead-reckoning)
        PayloadUpdateMsg lastPayload{};
        bool hasPayload = false;
    };

    /// ILinkAcceptor that routes new links into PeerNode's pending queue.
    struct PeerAcceptor : Rtt::ILinkAcceptor
    {
        struct PendingLink
        {
            std::shared_ptr<Rtt::ILink> link;
            std::shared_ptr<LinkBridge> bridge;
        };

        std::mutex mutex;
        std::queue<PendingLink> pendingLinks;

        /// Must be set before any links are accepted.
        SynTm::IClock* clock = nullptr;

        Rtt::LinkHandler OnLink(Rtt::LinkResult result) override
        {
            if (!result) {
                return {};
            }
            auto link = *result;
            auto bridge = std::make_shared<LinkBridge>();
            {
                std::scoped_lock lock(mutex);
                pendingLinks.push({.link=link, .bridge=bridge});
            }
            return bridge->MakeHandler(clock);
        }

        [[nodiscard]] std::queue<PendingLink> DrainPending()
        {
            std::scoped_lock lock(mutex);
            std::queue<PendingLink> result;
            std::swap(result, pendingLinks);
            return result;
        }
    };

    /// Coroutine orchestrator for a single peer.
    ///
    /// Runs as an Exec::RunTask<int> on an Exec::Domain. Owns transport,
    /// connector, acceptor, and per-link states. Drives SynTm probe exchange,
    /// payload broadcasting, and message dispatch on a 50ms tick loop.
    class PeerNode
    {
        Log::Logger _logger;

    public:
        PeerNode(
            Peer& peer,
            std::shared_ptr<Rtt::ITransport> transport)
            : _logger("PeerNode", peer.logger)
            , _peer(peer)
            , _transport(std::move(transport))
            , _acceptor(std::make_shared<PeerAcceptor>())
        {
            _acceptor->clock = &_peer.clock;
        }

        [[nodiscard]] Peer& GetPeer() { return _peer; }
        [[nodiscard]] const Peer& GetPeer() const { return _peer; }

        /// Get all link states (for UI rendering).
        [[nodiscard]] const std::unordered_map<std::string, PeerLinkState>& Links() const
        {
            return _links;
        }

        /// Request connection to a remote peer (thread-safe, enqueues).
        void ConnectTo(const std::string& remotePeerId)
        {
            std::scoped_lock lock(_connectMutex);
            _pendingConnects.push(remotePeerId);
        }

        /// Request disconnection from a remote peer (thread-safe, enqueues).
        void DisconnectFrom(const std::string& remotePeerId)
        {
            std::scoped_lock lock(_connectMutex);
            _pendingDisconnects.push(remotePeerId);
        }

        /// The main async coroutine.
        Exec::RunTask<int> Run()
        {
            _logger.Info("start processing");

            const auto sched = co_await stdexec::read_env(stdexec::get_scheduler);

            // Open transport and get connector for outbound connections.
            _connector = _transport->Open(_acceptor);

            constexpr auto tick = std::chrono::milliseconds(200);

            // ReSharper disable once CppDFAEndlessLoop
            while (true) {
                co_await exec::schedule_after(sched, tick);

                // Process pending connect/disconnect requests.
                ProcessConnectRequests();

                // Accept newly arrived links.
                AcceptNewLinks();

                // Process messages from all links.
                ProcessAllMessages();

                // Probe peers for time sync.
                ProbeAll();

                // Broadcast payload to all links.
                BroadcastPayload();

                // Remove disconnected links.
                RemoveDisconnected();
            }

            co_return 0;
        }

    private:
        void ProcessConnectRequests()
        {
            std::queue<std::string> connects;
            std::queue<std::string> disconnects;
            {
                std::scoped_lock lock(_connectMutex);
                std::swap(connects, _pendingConnects);
                std::swap(disconnects, _pendingDisconnects);
            }

            while (!connects.empty()) {
                auto& target = connects.front();
                if (_connector) {
                    _logger.Info("connecting to {}", target);
                    _connector->Connect({target});
                }
                connects.pop();
            }

            while (!disconnects.empty()) {
                auto& target = disconnects.front();
                auto it = _links.find(target);
                if (it != _links.end() && it->second.link) {
                    _logger.Info("disconnecting from {}", target);
                    it->second.link->Disconnect();
                    it->second.link = nullptr;
                }
                disconnects.pop();
            }
        }

        void AcceptNewLinks()
        {
            auto pending = _acceptor->DrainPending();
            while (!pending.empty()) {
                auto& p = pending.front();
                auto remotePeerId = p.link->RemoteId().value;
                _logger.Info("link accepted to {}", remotePeerId);

                _peer.consensus.AddPeer(remotePeerId);
                _links[remotePeerId] = PeerLinkState{
                    .remotePeerId = remotePeerId,
                    .link = std::move(p.link),
                    .bridge = std::move(p.bridge),
                };
                pending.pop();
            }
        }

        void ProcessAllMessages()
        {
            for (auto& [peerId, ls] : _links) {
                if (!ls.link) {
                    continue;
                }
                auto msgs = ls.bridge->Drain();
                while (!msgs.empty()) {
                    auto& msg = msgs.front();
                    if (!msg.data) {
                        ls.link = nullptr;
                        _logger.Info("link disconnected from {}", peerId);
                        break;
                    }
                    HandleMessage(peerId, *msg.data, msg.receivedAt);
                    msgs.pop();
                }
            }
        }

        void HandleMessage(
            const std::string& fromPeerId,
            std::span<const std::byte> data,
            SynTm::Ticks receivedAt)
        {
            auto parsed = ParseMessage(data);
            if (!parsed) {
                return;
            }

            switch (parsed->type) {
                case MsgType::SyncProbe:
                    HandleSyncProbe(fromPeerId, parsed->payload, receivedAt);
                    break;
                case MsgType::PayloadUpdate:
                    HandlePayloadUpdate(fromPeerId, parsed->payload);
                    break;
            }
        }

        void HandleSyncProbe(
            const std::string& fromPeerId,
            std::span<const std::byte> payload,
            SynTm::Ticks receivedAt)
        {
            auto syncMsg = SynTm::ParseSyncMessage(payload);
            if (!syncMsg || !syncMsg->pulse) {
                return;
            }

            // Always process the remote epoch.
            _peer.consensus.HandleRemoteEpoch(syncMsg->epoch);

            auto replyOpt = _peer.consensus.HandleSyncPulse(
                fromPeerId, *syncMsg->pulse, receivedAt, syncMsg->epoch);

            if (!replyOpt) {
                return;
            }

            // Only Passive (higher id) sends the reply immediately.
            const bool iAmPassive = !SynTm::Consensus::IsActivePeer(_peer.peerId, fromPeerId);
            if (!iAmPassive) {
                return;
            }

            auto epoch = _peer.consensus.OurEpochInfo();
            std::array<std::byte, 128> raw{};
            auto n = SynTm::WriteSyncPulse(raw, epoch, *replyOpt);
            if (n > 0) {
                auto wrapped = WrapSyncProbe(std::span<const std::byte>(raw.data(), n));
                SendTo(fromPeerId, wrapped);
            }
        }

        void HandlePayloadUpdate(const std::string& fromPeerId, std::span<const std::byte> payload)
        {
            auto update = ReadPayload<PayloadUpdateMsg>(payload);
            if (!update) {
                return;
            }
            auto it = _links.find(fromPeerId);
            if (it != _links.end()) {
                it->second.lastPayload = *update;
                it->second.hasPayload = true;
            }
        }

        void ProbeAll()
        {
            for (auto& [peerId, ls] : _links) {
                if (!ls.link) {
                    continue;
                }
                if (!_peer.consensus.ShouldInitiateProbe(_peer.peerId, peerId)) {
                    continue;
                }
                auto pulseOpt = _peer.consensus.MakePulse(peerId);
                if (!pulseOpt) {
                    continue;
                }
                auto epoch = _peer.consensus.OurEpochInfo();
                std::array<std::byte, 128> raw{};
                auto n = SynTm::WriteSyncPulse(raw, epoch, *pulseOpt);
                if (n > 0) {
                    auto wrapped = WrapSyncProbe(std::span<const std::byte>(raw.data(), n));
                    SendTo(peerId, wrapped);
                }
            }
        }

        void BroadcastPayload()
        {
            auto syncNow = _peer.syncClock.Now();
            auto msg = SerializePayloadUpdate({
                .x = _peer.position.x,
                .y = _peer.position.y,
                .vx = _peer.velocity.x,
                .vy = _peer.velocity.y,
                .syncTimeNs = syncNow.count(),
            });
            for (auto& [peerId, ls] : _links) {
                if (!ls.link) {
                    continue;
                }
                SendTo(peerId, msg);
            }
        }

        void SendTo(const std::string& peerId, std::span<const std::byte> data)
        {
            auto it = _links.find(peerId);
            if (it == _links.end() || !it->second.link) {
                return;
            }
            auto copy = std::vector<std::byte>(data.begin(), data.end());
            it->second.link->Send([copy = std::move(copy)](std::span<std::byte> buf) -> std::size_t {
                auto n = std::min(buf.size(), copy.size());
                std::memcpy(buf.data(), copy.data(), n);
                return n;
            });
        }

        void RemoveDisconnected()
        {
            for (auto it = _links.begin(); it != _links.end(); ) {
                if (!it->second.link) {
                    _logger.Info("removing link to {}", it->first);
                    _peer.consensus.RemovePeer(it->first);
                    it = _links.erase(it);
                } else {
                    ++it;
                }
            }
        }

        Peer& _peer;
        std::shared_ptr<Rtt::ITransport> _transport;
        std::shared_ptr<PeerAcceptor> _acceptor;
        std::shared_ptr<Rtt::IConnector> _connector;

        std::unordered_map<std::string, PeerLinkState> _links;

        std::mutex _connectMutex;
        std::queue<std::string> _pendingConnects;
        std::queue<std::string> _pendingDisconnects;
    };
}
