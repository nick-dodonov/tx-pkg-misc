#pragma once
#include "Detail/ConsoleBuffer.h"
#include "Detail/ConsoleSink.h"
#include <memory>

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
        void ExecuteCommand(const std::string& command);
        
        std::shared_ptr<Detail::ConsoleBuffer> _buffer;
        std::shared_ptr<Detail::ConsoleSinkMt> _sink;
        
        bool _visible = false;
        float _animationProgress = 0.0f;  // 0.0 = hidden, 1.0 = fully visible
        bool _autoScroll = true;
        bool _shouldFocusInput = false;
        float _consoleHeight = 0.0f;  // User-defined height, 0 = use default
        
        static constexpr float ANIMATION_SPEED = 16.0f;  // Units per second
        static constexpr float CONSOLE_HEIGHT_RATIO = 0.5f;  // 50% of window height
    };

} // namespace Im
