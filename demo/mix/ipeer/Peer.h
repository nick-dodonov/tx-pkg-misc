#pragma once

#include "SynTm/Clock.h"
#include "SynTm/Consensus.h"
#include "SynTm/SyncClock.h"

#include "imgui.h"
#include <cmath>
#include <string>

namespace Demo
{
    /// Color palette for visually distinguishing peers.
    inline constexpr ImVec4 PeerColors[] = {
        {0.26f, 0.59f, 0.98f, 1.0f}, // Blue
        {0.98f, 0.39f, 0.26f, 1.0f}, // Red-orange
        {0.26f, 0.85f, 0.42f, 1.0f}, // Green
        {0.95f, 0.75f, 0.20f, 1.0f}, // Yellow
        {0.80f, 0.40f, 0.90f, 1.0f}, // Purple
        {0.20f, 0.85f, 0.85f, 1.0f}, // Cyan
        {0.95f, 0.55f, 0.65f, 1.0f}, // Pink
        {0.60f, 0.80f, 0.30f, 1.0f}, // Lime
    };
    inline constexpr int PeerColorCount = sizeof(PeerColors) / sizeof(PeerColors[0]);

    struct Vec2
    {
        float x = 0.0f;
        float y = 0.0f;
    };

    /// A single peer with identity, animated position, sync state, and velocity.
    struct Peer
    {
        int id = 0;
        std::string name;
        std::string peerId; ///< String ID used by transport/signaling.
        ImVec4 color{1, 1, 1, 1};

        Vec2 position{};
        Vec2 velocity{};

        // SynTm time synchronization stack (one per peer).
        SynTm::AppClock clock;
        SynTm::Consensus consensus{clock};
        SynTm::SyncClock syncClock{consensus};

        /// Update position using Lissajous curve animation.
        /// Each peer gets a unique pattern based on its id.
        void UpdatePosition(float dt, double sessionTime)
        {
            float phase = static_cast<float>(id) * 1.17f;
            float freqX = 0.5f + static_cast<float>(id % 3) * 0.3f;
            float freqY = 0.7f + static_cast<float>(id % 4) * 0.2f;

            float t = static_cast<float>(sessionTime) + phase;
            float radius = 0.35f;

            float newX = radius * std::sin(freqX * t);
            float newY = radius * std::cos(freqY * t);

            if (dt > 0.0f) {
                velocity.x = (newX - position.x) / dt;
                velocity.y = (newY - position.y) / dt;
            }

            position.x = newX;
            position.y = newY;

            // Refresh atomic snapshot for thread-safe reads.
            syncClock.Update();
        }
    };
}
