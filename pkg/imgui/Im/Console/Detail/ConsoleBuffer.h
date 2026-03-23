#pragma once
#include <spdlog/spdlog.h>
#include <deque>
#include <mutex>
#include <string>

namespace Im::Detail
{
    // Ring buffer for storing log entries
    class ConsoleBuffer
    {
    public:
        struct LogEntry
        {
            spdlog::level::level_enum level;
            std::string message;
            std::string logger_name;
        };

        explicit ConsoleBuffer(size_t max_entries = 1000)
            : _maxEntries(max_entries)
        {
        }

        void AddEntry(spdlog::level::level_enum level, std::string message, std::string logger_name)
        {
            std::lock_guard<std::mutex> lock(_mutex);
            
            if (_entries.size() >= _maxEntries) {
                _entries.pop_front();
            }
            
            _entries.push_back({level, std::move(message), std::move(logger_name)});
        }

        void Clear()
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _entries.clear();
        }

        template<typename Func>
        void ForEach(Func&& func) const
        {
            std::lock_guard<std::mutex> lock(_mutex);
            for (const auto& entry : _entries) {
                func(entry);
            }
        }

        size_t Size() const
        {
            std::lock_guard<std::mutex> lock(_mutex);
            return _entries.size();
        }

    private:
        size_t _maxEntries;
        std::deque<LogEntry> _entries;
        mutable std::mutex _mutex;
    };

} // namespace Im::Detail
