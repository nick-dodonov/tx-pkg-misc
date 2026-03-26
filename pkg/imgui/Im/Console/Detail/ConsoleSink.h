#pragma once
#include "ConsoleBuffer.h"
#include "Log/Sink.h"
#include "Log/Details/Format.h"

namespace Im::Detail
{
    // Custom sink that writes to ConsoleBuffer
    template<typename Mutex>
    class ConsoleSink : public Log::Detail::BaseSink<Mutex>
    {
    public:
        explicit ConsoleSink(std::shared_ptr<ConsoleBuffer> buffer)
            : _buffer(std::move(buffer))
        {
            this->set_formatter(Log::Detail::MakeDefaultFormatter());
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
            
            _buffer->AddEntry(
                msg.level, 
                std::move(message), 
                std::string(msg.logger_name.data(), 
                msg.logger_name.size()));
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

} // namespace Im::Detail
