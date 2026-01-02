#include "FpsCounter.h"
#include <numeric>
#include <algorithm>

FpsCounter::FpsCounter(size_t capacity)
    : _capacity(capacity)
    , _frameTimes(capacity)
{
}

void FpsCounter::AddFrame(float deltaSeconds)
{
    if (deltaSeconds > 0.0f) {
        _frameTimes[_index] = deltaSeconds;
        _index = (_index + 1) % _capacity;
        _sampleCount = std::min(_sampleCount + 1, _capacity);
    }
}

float FpsCounter::GetAverageFps() const
{
    if (_sampleCount == 0) {
        return 0.0f;
    }
    float totalTime = std::accumulate(_frameTimes.begin(), _frameTimes.begin() + static_cast<int>(_sampleCount), 0.0f);
    if (totalTime == 0.0f) {
        return 0.0f;
    }
    return static_cast<float>(_sampleCount) / totalTime;
}
