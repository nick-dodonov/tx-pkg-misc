#pragma once
#include "../ConsoleBuffer.h"
#include <spdlog/sinks/base_sink.h>
#include <memory>

namespace Im::Detail
{
    // Custom spdlog sink that writes to ConsoleBuffer
    template<typename Mutex>
    class ConsoleSink : public spdlog::sinks::base_sink<Mutex>
    {
    public:
        explicit ConsoleSink(std::shared_ptr<Im::ConsoleBuffer> buffer)
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
        std::shared_ptr<Im::ConsoleBuffer> _buffer;
    };

    using ConsoleSinkMt = ConsoleSink<std::mutex>;
    using ConsoleSinkSt = ConsoleSink<spdlog::details::null_mutex>;

} // namespace Im::Detail
