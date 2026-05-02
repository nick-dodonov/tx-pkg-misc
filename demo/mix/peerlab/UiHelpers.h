#pragma once

#include "imgui.h"
#include <cmath>

namespace Demo
{
    inline ImVec4 GetDeltaCol(double deltaMs, double thresholdGood = 1.0, double thresholdWarn = 5.0)
    {
        // Delta epoch sync quality colors
        static constexpr ImVec4 ColDeltaGood   = {0.26f, 0.85f, 0.42f, 1.0f}; // green  — < 5 ms
        static constexpr ImVec4 ColDeltaWarn   = {0.95f, 0.75f, 0.20f, 1.0f}; // yellow — < 20 ms
        static constexpr ImVec4 ColDeltaBad    = {0.98f, 0.39f, 0.26f, 1.0f}; // red    — >= 20 ms

        const auto absDelta = std::abs(deltaMs);
        return absDelta < thresholdGood
            ? ColDeltaGood
            : absDelta < thresholdWarn //NOLINT(readability-avoid-nested-conditional-operator)
                ? ColDeltaWarn
                : ColDeltaBad;
    }
}
