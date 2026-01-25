#include "QuakeConsole.h"
#include "imgui.h"
#include <Log/Log.h>
#include <Log/Sink.h>
#include <array>
#include <string>

namespace Im
{
    static constexpr size_t MAX_BUFFER_SIZE = 1000;
    static constexpr float ANIMATION_SPEED = 16.0f;     // Units per second
    static constexpr float CONSOLE_HEIGHT_RATIO = 0.7f; // % of window height
    static constexpr float CONSOLE_FONT_SCALE = 0.9f;   // Scale down font for better readability
    static constexpr float CONSOLE_LINE_SPACING = 2.0f; // Reduced line spacing for compact output

    QuakeConsole::QuakeConsole(bool initiallyVisible)
        : _buffer(std::make_shared<Detail::ConsoleBuffer>(MAX_BUFFER_SIZE))
        , _sink(std::make_shared<Detail::ConsoleSinkMt>(_buffer))
        , _visible(initiallyVisible)
        , _animationProgress(initiallyVisible ? 1.0f : 0.0f)
        , _shouldFocusInput(initiallyVisible)
    {}

    QuakeConsole::~QuakeConsole()
    {
        Log::Detail::RemoveSink(_sink);
    }

    void QuakeConsole::Initialize()
    {
        Log::Detail::AddSink(_sink);

        // Find monospace font (should be the second font loaded by Deputy)
        auto& io = ImGui::GetIO();
        if (io.Fonts->Fonts.Size > 1) {
            _monoFont = io.Fonts->Fonts[1];
        }
    }

    void QuakeConsole::Toggle()
    {
        _visible = !_visible;
        if (_visible) {
            _shouldFocusInput = true;
        }
    }

    void QuakeConsole::Clear()
    {
        _buffer->Clear();
    }

    ImVec4 QuakeConsole::GetColorForLogLevel(const spdlog::level::level_enum level)
    {
        switch (level) {
            case spdlog::level::trace:
                return {0.5f, 0.5f, 0.5f, 1.0f}; // Gray
            case spdlog::level::debug:
                // return {0.4f, 0.8f, 1.0f, 1.0f};  // Cyan
                return {0.3f, 0.65f, 0.79f, 1.0f}; // Darker cyan
            case spdlog::level::info:
                // return {0.8f, 0.8f, 0.8f, 1.0f};  // Light gray
                return {0.34f, 0.72f, 0.50f, 1.0f}; // Light green
            case spdlog::level::warn:
                return {1.0f, 1.0f, 0.0f, 1.0f}; // Yellow
            case spdlog::level::err:
                return {1.0f, 0.4f, 0.4f, 1.0f}; // Red
            case spdlog::level::critical:
                return {1.0f, 0.0f, 0.0f, 1.0f}; // Bright red
            default:
                return {1.0f, 1.0f, 1.0f, 1.0f}; // White
        }
    }

    bool QuakeConsole::IsLogLevelEnabled(const spdlog::level::level_enum level) const
    {
        switch (level) {
            case spdlog::level::trace:
                return _filterTrace;
            case spdlog::level::debug:
                return _filterDebug;
            case spdlog::level::info:
                return _filterInfo;
            case spdlog::level::warn:
                return _filterWarn;
            case spdlog::level::err:
                return _filterError;
            case spdlog::level::critical:
                return _filterCritical;
            default:
                return true;
        }
    }

    void QuakeConsole::RenderFilters()
    {
        // Helper lambda for flat toggle buttons
        auto ToggleButton = [](const char* label, bool* value, spdlog::level::level_enum level, const char* tooltip = nullptr) {
            const ImVec4 levelColor = GetColorForLogLevel(level);
            
            if (*value) {
                // Active: thick colored border
                ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 2.0f);
                ImGui::PushStyleColor(ImGuiCol_Border, levelColor);
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.2f, 0.5f));
            } else {
                // Inactive: no border, dark background
                ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
                ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.15f, 0.15f, 0.5f));
            }
            
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered));
            ImGui::PushStyleColor(ImGuiCol_Text, levelColor);
            
            // Fixed width for consistent button sizes
            const ImVec2 buttonSize(20.0f, 0.0f);
            if (ImGui::Button(label, buttonSize)) {
                *value = !*value;
            }
            
            ImGui::PopStyleColor(4);
            ImGui::PopStyleVar();
            
            if (tooltip && ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", tooltip);
            }
        };

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Filter:");

        // Log level toggles
        ImGui::SameLine();
        ToggleButton("T", &_filterTrace, spdlog::level::trace, "Trace");
        ImGui::SameLine();
        ToggleButton("D", &_filterDebug, spdlog::level::debug, "Debug");
        ImGui::SameLine();
        ToggleButton("I", &_filterInfo, spdlog::level::info, "Info");
        ImGui::SameLine();
        ToggleButton("W", &_filterWarn, spdlog::level::warn, "Warn");
        ImGui::SameLine();
        ToggleButton("E", &_filterError, spdlog::level::err, "Error");
        ImGui::SameLine();
        ToggleButton("C", &_filterCritical, spdlog::level::critical, "Critical");

        ImGui::SameLine();
        ImGui::Dummy(ImVec2(20, 0));

        // Clear button ✖
        ImGui::SameLine();
        const ImVec2 buttonSize(20.0f, 0.0f);
        if (ImGui::Button("✖", buttonSize)) {
            Clear();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Clear");
        }

        // Auto-scroll toggle ⬇
        ImGui::SameLine();
        if (_autoScroll) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.2f, 0.5f));
        }
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered));
        if (ImGui::Button("⬇")) {
            _autoScroll = !_autoScroll;
        }
        ImGui::PopStyleColor(2);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Auto-scroll");
        }
    }

    void QuakeConsole::RenderLogOutput()
    {
        const float footerHeight = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
        
        ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footerHeight), ImGuiChildFlags_Borders, ImGuiWindowFlags_None);

        // Use monospace font for log output
        if (_monoFont) {
            ImGui::PushFont(_monoFont);
            ImGui::SetWindowFontScale(CONSOLE_FONT_SCALE);
        }

        // Reduce line spacing for compact output
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, CONSOLE_LINE_SPACING));

        // Display log entries with filter
        _buffer->ForEach([this](const Detail::ConsoleBuffer::LogEntry& entry) {
            if (!IsLogLevelEnabled(entry.level)) {
                return;
            }

            const ImVec4 color = GetColorForLogLevel(entry.level);
            ImGui::PushStyleColor(ImGuiCol_Text, color);
            ImGui::TextUnformatted(entry.message.c_str());
            ImGui::PopStyleColor();
        });

        ImGui::PopStyleVar(); // ItemSpacing

        // Auto-scroll to bottom
        if (_autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
            ImGui::SetScrollHereY(1.0f);
        }

        if (_monoFont) {
            ImGui::SetWindowFontScale(1.0f);  // BeginChild starts with scale 1.0f by default
            ImGui::PopFont();
        }

        ImGui::EndChild();
    }

    void QuakeConsole::RenderCommandInput()
    {
        static std::array<char, 256> inputBuf{};
        const ImGuiInputTextFlags inputFlags = ImGuiInputTextFlags_EnterReturnsTrue;

        ImGui::PushItemWidth(-1);
        if (ImGui::InputText("##ConsoleInput", inputBuf.data(), inputBuf.size(), inputFlags)) {
            // Handle command input
            if (inputBuf[0] != '\0') {
                std::string command(inputBuf.data());
                ExecuteCommand(command);
                inputBuf[0] = '\0';
            }
            // Keep focus on input after executing command
            ImGui::SetKeyboardFocusHere(-1);
        }
        ImGui::PopItemWidth();
    }

    void QuakeConsole::Render()
    {
        const float deltaTime = ImGui::GetIO().DeltaTime;

        // Animate console sliding down/up
        if (_visible && _animationProgress < 1.0f) {
            _animationProgress = std::min(1.0f, _animationProgress + ANIMATION_SPEED * deltaTime);
        } else if (!_visible && _animationProgress > 0.0f) {
            _animationProgress = std::max(0.0f, _animationProgress - ANIMATION_SPEED * deltaTime);
        }

        // Don't render if fully hidden
        if (_animationProgress <= 0.0f) {
            return;
        }

        // Get window dimensions
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        const float windowWidth = viewport->WorkSize.x;

        // Use saved height or default to 50% of screen
        if (_consoleHeight == 0.0f) {
            _consoleHeight = viewport->WorkSize.y * CONSOLE_HEIGHT_RATIO;
        }
        const float maxHeight = _consoleHeight;
        const float currentHeight = maxHeight * _animationProgress;

        // Position at top of screen
        ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, viewport->WorkPos.y), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(windowWidth, currentHeight), ImGuiCond_Always);
        ImGui::SetNextWindowSizeConstraints(ImVec2(windowWidth, 100.0f), ImVec2(windowWidth, viewport->WorkSize.y * 0.9f));

        // Console window style
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.85f));

        const ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;

        if (ImGui::Begin("QuakeConsole", nullptr, windowFlags)) {
            RenderFilters();
            RenderLogOutput();
            RenderCommandInput();

            // Set focus to input when console opens
            if (_shouldFocusInput && _animationProgress > 0.95f) {
                ImGui::SetKeyboardFocusHere(-1);
                _shouldFocusInput = false;
            }
        }

        // Save user-defined height when fully visible
        if (_visible && _animationProgress >= 1.0f) {
            const auto windowSize = ImGui::GetWindowSize();
            if (windowSize.y > 0.0f) {
                _consoleHeight = windowSize.y;
            }
        }

        ImGui::End();

        ImGui::PopStyleColor();
        ImGui::PopStyleVar(3);
    }

    void QuakeConsole::ExecuteCommand(const std::string& command)
    {
        // Log the command
        Log::Info("> {}", command);

        // Process commands
        if (command == "clear") {
            Clear();
        } else if (command == "help") {
            Log::Info("Available commands:");
            Log::Info("  help  - Show this help message");
            Log::Info("  clear - Clear console output");
            Log::Info("  test  - Test all log levels");
        } else if (command == "test") {
            Log::Trace("[TEST] This is a TRACE message");
            Log::Debug("[TEST] This is a DEBUG message");
            Log::Info("[TEST] This is an INFO message");
            Log::Warn("[TEST] This is a WARNING message");
            Log::Error("[TEST] This is an ERROR message");
            Log::Fatal("[TEST] This is a CRITICAL message");
        } else {
            Log::Warn("Unknown command: '{}'. Type 'help' for available commands.", command);
        }
    }

} // namespace Im
