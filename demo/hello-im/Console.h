#pragma once
#include <spdlog/spdlog.h>
#include <spdlog/sinks/base_sink.h>
#include <deque>
#include <mutex>
#include <string>
#include <memory>

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

// Custom spdlog sink that writes to ConsoleBuffer
template<typename Mutex>
class ConsoleSink : public spdlog::sinks::base_sink<Mutex>
{
public:
    explicit ConsoleSink(std::shared_ptr<ConsoleBuffer> buffer)
        : _buffer(std::move(buffer))
    {
    }

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override
    {
        spdlog::memory_buf_t formatted;
        this->formatter_->format(msg, formatted);
        
        std::string message(formatted.data(), formatted.size());
        
        // Remove trailing newline if present
        if (!message.empty() && message.back() == '\n') {
            message.pop_back();
        }
        
        _buffer->AddEntry(msg.level, std::move(message), std::string(msg.logger_name.data(), msg.logger_name.size()));
    }

    void flush_() override
    {
        // Nothing to flush for in-memory buffer
    }

private:
    std::shared_ptr<ConsoleBuffer> _buffer;
};

using ConsoleSinkMt = ConsoleSink<std::mutex>;
using ConsoleSinkSt = ConsoleSink<spdlog::details::null_mutex>;

// Quake-style console widget for ImGui
class QuakeConsole
{
public:
    explicit QuakeConsole(bool initiallyVisible = false);
    ~QuakeConsole();

    // Initialize and attach to spdlog
    void Initialize();
    
    // Toggle console visibility
    void Toggle();
    void Show() { _visible = true; }
    void Hide() { _visible = false; }
    bool IsVisible() const { return _visible; }

    // Render the console window (call in ImGui context)
    void Render();

    // Clear all log entries
    void Clear();

private:
    void ExecuteCommand(const std::string& command);
    std::shared_ptr<ConsoleBuffer> _buffer;
    std::shared_ptr<ConsoleSinkMt> _sink;
    
    bool _visible = false;
    float _animationProgress = 0.0f;  // 0.0 = hidden, 1.0 = fully visible
    bool _autoScroll = true;
    
    static constexpr float ANIMATION_SPEED = 8.0f;  // Units per second
    static constexpr float CONSOLE_HEIGHT_RATIO = 0.5f;  // 50% of window height
};
