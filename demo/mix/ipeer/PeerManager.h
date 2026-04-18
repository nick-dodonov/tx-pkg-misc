#pragma once

#include "Peer.h"
#include "PeerLink.h"
#include "Log/Log.h"

#include <memory>
#include <vector>

namespace IPeer
{
    /// Central registry of peers and their connections.
    /// Drives per-frame simulation and message exchange.
    class PeerManager
    {
    public:
        Peer& CreatePeer(std::string name = "")
        {
            int id = _nextId++;
            if (name.empty()) {
                name = "Peer-" + std::to_string(id);
            }
            auto& peer = _peers.emplace_back(std::make_unique<Peer>());
            peer->id = id;
            peer->name = std::move(name);
            peer->color = PeerColors[id % PeerColorCount];
            Log::Info("created {} (id={})", peer->name, peer->id);
            return *peer;
        }

        void RemovePeer(int peerId)
        {
            // Remove all links involving this peer first
            std::erase_if(_links, [peerId](const auto& link) {
                return link->GetPeerA().id == peerId || link->GetPeerB().id == peerId;
            });
            std::erase_if(_peers, [peerId](const auto& p) {
                return p->id == peerId;
            });
            Log::Info("removed peer id={}", peerId);
        }

        PeerLink& Connect(Peer& a, Peer& b)
        {
            auto& link = _links.emplace_back(std::make_unique<PeerLink>(a, b));
            Log::Info("connected {} <-> {}", a.name, b.name);
            return *link;
        }

        void Disconnect(Peer& a, Peer& b)
        {
            std::erase_if(_links, [&](const auto& link) {
                return (&link->GetPeerA() == &a && &link->GetPeerB() == &b)
                    || (&link->GetPeerA() == &b && &link->GetPeerB() == &a);
            });
            Log::Info("disconnected {} <-> {}", a.name, b.name);
        }

        /// Advance all peer positions and process all links.
        void Update(float dt, double sessionTime)
        {
            for (auto& peer : _peers) {
                peer->UpdatePosition(dt, sessionTime);
            }
            for (auto& link : _links) {
                link->ProcessFrame(dt, sessionTime);
            }
        }

        [[nodiscard]] Peer* FindPeer(int id) const
        {
            for (const auto& p : _peers) {
                if (p->id == id) {
                    return p.get();
                }
            }
            return nullptr;
        }

        [[nodiscard]] PeerLink* FindLink(int peerAId, int peerBId) const
        {
            for (const auto& link : _links) {
                bool match = (link->GetPeerA().id == peerAId && link->GetPeerB().id == peerBId)
                          || (link->GetPeerA().id == peerBId && link->GetPeerB().id == peerAId);
                if (match) {
                    return link.get();
                }
            }
            return nullptr;
        }

        [[nodiscard]] bool AreConnected(int peerAId, int peerBId) const
        {
            return FindLink(peerAId, peerBId) != nullptr;
        }

        /// Get all links involving a specific peer.
        [[nodiscard]] std::vector<PeerLink*> GetLinksFor(int peerId) const
        {
            std::vector<PeerLink*> result;
            for (const auto& link : _links) {
                if (link->GetPeerA().id == peerId || link->GetPeerB().id == peerId) {
                    result.push_back(link.get());
                }
            }
            return result;
        }

        [[nodiscard]] const std::vector<std::unique_ptr<Peer>>& GetPeers() const { return _peers; }
        [[nodiscard]] const std::vector<std::unique_ptr<PeerLink>>& GetLinks() const { return _links; }

    private:
        std::vector<std::unique_ptr<Peer>> _peers;
        std::vector<std::unique_ptr<PeerLink>> _links;
        int _nextId = 1;
    };
}
