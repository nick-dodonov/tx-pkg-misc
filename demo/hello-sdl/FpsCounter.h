#pragma once
#include <vector>
#include <cstddef>

/// Helper structure to calculate average FPS over a sliding window
struct FpsCounter
{
    static constexpr size_t DefaultCapacity = 30;

    explicit FpsCounter(size_t capacity = DefaultCapacity);

    void AddFrame(float deltaSeconds);

    float GetAverageFps() const;

private:
    size_t _capacity;
    std::vector<float> _frameTimes;
    size_t _index = 0;
    size_t _sampleCount = 0;
};
