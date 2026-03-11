#pragma once
#include "Detail/ConsoleBuffer.h"
#include "Detail/ConsoleSink.h"
#include <memory>

struct ImVec4;
struct ImFont;

namespace Im
{
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
        [[nodiscard]] bool IsVisible() const { return _visible; }

        // Render the console window (call in ImGui context)
        void Render();

        // Clear all log entries
        void Clear();

    private:
        // Focus target for TAB navigation
        enum class ConsoleFocus
        {
            None,
            CommandInput,
            FilterInput
        };

        void ExecuteCommand(const std::string& command);

        [[nodiscard]] bool IsLogLevelEnabled(spdlog::level::level_enum level) const;
        
        void RenderFilters();
        void RenderLogOutput();
        void RenderCommandInput();

        std::shared_ptr<Detail::ConsoleBuffer> _buffer;
        std::shared_ptr<Detail::ConsoleSinkMt> _sink;
        ImFont* _monoFont = nullptr;  // Monospace font for log output
        
        bool _visible = false;
        float _animationProgress = 0.0f;  // 0.0 = hidden, 1.0 = fully visible
        bool _autoScroll = true;
        ConsoleFocus _focusTarget = ConsoleFocus::None;
        float _consoleHeight = 0.0f;  // User-defined height, 0 = use default
        
        // Log level filters
        bool _filterTrace = true;
        bool _filterDebug = true;
        bool _filterInfo = true;
        bool _filterWarn = true;
        bool _filterError = true;
        bool _filterCritical = true;
        
        // Text filter
        std::array<char, 256> _filterText{};
    };

} // namespace Im
