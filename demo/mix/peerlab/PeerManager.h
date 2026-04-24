#pragma once

#include "Peer.h"
#include "PeerNode.h"
#include "TransportFactory.h"

#include "Exec/Domain.h"
#include "Log/Log.h"
#include "RunLoop/CompositeHandler.h"

#include <memory>
#include <string>
#include <vector>

namespace Demo
{
    /// Managed peer entry: peer data + coroutine node + exec domain.
    struct ManagedPeer
    {
        std::unique_ptr<Peer> peer;
        std::unique_ptr<PeerNode> node;
        std::shared_ptr<Exec::Domain> domain;
    };

    /// Central registry of peers and their transport/sync infrastructure.
    ///
    /// Creates peers with PeerNode coroutines running on Exec::Domain,
    /// integrated into the CompositeHandler for frame-driven scheduling.
    class PeerManager
    {
    public:
        PeerManager(RunLoop::CompositeHandler& composite, TransportFactory& transportFactory)
            : _composite(composite)
            , _transportFactory(transportFactory)
        {}

        Peer& CreatePeer(std::string name = "")
        {
            auto id = _nextId++;
            if (name.empty()) {
                name = "Peer-" + std::to_string(id);
            }
            auto peer = std::make_unique<Peer>(id, std::move(name), "p" + std::to_string(id));

            // Create transport and PeerNode coroutine.
            auto transport = _transportFactory.CreateTransport(peer->peerId, peer->logger);
            auto node = std::make_unique<PeerNode>(*peer, std::move(transport));

            // Create Domain from the coroutine and register with composite handler.
            auto domain = std::make_shared<Exec::Domain>(node->Run(), Exec::Domain::Options{
                .parentLogger = peer->logger
            });
            _composite.Add(*domain);

            Log::Info("created {} (id={}, peerId={})", peer->name, peer->id, peer->peerId);

            auto& entry = _entries.emplace_back(
                ManagedPeer{
                    .peer = std::move(peer),
                    .node = std::move(node),
                    .domain = std::move(domain),
                }
            );
            return *entry.peer;
        }

        void RemovePeer(int peerId)
        {
            auto it = std::find_if(_entries.begin(), _entries.end(), [peerId](const auto& e) { return e.peer->id == peerId; });
            if (it == _entries.end()) {
                return;
            }
            Log::Info("removing peer {} (id={})", it->peer->name, peerId);
            _composite.Remove(*it->domain);
            _entries.erase(it);
        }

        /// Request a connection from initiator to responder.
        ///
        /// Only the initiator sends a WebRTC offer; the responder waits for it and
        /// answers.  Calling ConnectTo on both sides simultaneously was the root
        /// cause of glare (concurrent cross-offers).
        void Connect(Peer& initiator, Peer& responder) const
        {
            auto* initiatorNode = FindNode(initiator.id);
            if (!initiatorNode) {
                return;
            }
            initiatorNode->ConnectTo(responder.peerId);
            Log::Info("connecting {} -> {}", initiator.name, responder.name);
        }

        /// Request disconnection initiated by one peer; the other side detects it via onDisconnected.
        void Disconnect(Peer& initiator, Peer& responder) const
        {
            auto* initiatorNode = FindNode(initiator.id);
            if (!initiatorNode) {
                return;
            }
            initiatorNode->DisconnectFrom(responder.peerId);
            Log::Info("disconnecting {} -> {}", initiator.name, responder.name);
        }

        /// Advance all peer positions (physics simulation on UI thread).
        void Update(float dt, double sessionTime)
        {
            for (auto& entry : _entries) {
                entry.peer->UpdatePosition(dt, sessionTime);
            }
        }

        /// Check if two peers have an active link between them.
        [[nodiscard]] bool AreConnected(int peerAId, int peerBId) const
        {
            auto* nodeA = FindNode(peerAId);
            if (!nodeA) {
                return false;
            }
            auto peerBPeerId = FindPeerIdStr(peerBId);
            if (peerBPeerId.empty()) {
                return false;
            }
            return nodeA->Links().contains(peerBPeerId);
        }

        [[nodiscard]] Peer* FindPeer(int id) const
        {
            for (const auto& e : _entries) {
                if (e.peer->id == id) {
                    return e.peer.get();
                }
            }
            return nullptr;
        }

        [[nodiscard]] PeerNode* FindNode(int peerId) const
        {
            for (const auto& e : _entries) {
                if (e.peer->id == peerId) {
                    return e.node.get();
                }
            }
            return nullptr;
        }

        [[nodiscard]] const std::vector<ManagedPeer>& Entries() const { return _entries; }
        [[nodiscard]] TransportMode GetTransportMode() const { return _transportFactory.Mode(); }

    private:
        [[nodiscard]] std::string FindPeerIdStr(int id) const
        {
            auto* peer = FindPeer(id);
            return peer ? peer->peerId : "";
        }

        std::vector<ManagedPeer> _entries;
        RunLoop::CompositeHandler& _composite;
        TransportFactory& _transportFactory;
        int _nextId = 1;
    };
}
