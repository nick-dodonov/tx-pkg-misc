#include "QuakeConsole.h"
#include "Log/Log.h"
#include "Log/Sink.h"

#include "imgui.h"
#include "imgui_internal.h"
#include <array>
#include <string>

namespace Im
{
    static constexpr size_t MAX_BUFFER_SIZE = 1000;
    static constexpr float ANIMATION_SPEED = 16.0f;                             // Units per second
    static constexpr float CONSOLE_HEIGHT_RATIO = 0.7f;                         // % of window height
    static constexpr float CONSOLE_FONT_SCALE = 0.9f;                           // Scale down font for better readability
    static constexpr float CONSOLE_LINE_SPACING = 2.0f;                         // Reduced line spacing for compact output
    static constexpr float CONSOLE_MIN_HEIGHT = 100.0f;                         // Minimum console height in pixels
    static constexpr float CONSOLE_MAX_HEIGHT_RATIO = 0.9f;                     // Maximum console height as % of screen
    static constexpr ImVec4 CONSOLE_BG_COLOR = ImVec4(0.0f, 0.0f, 0.0f, 0.85f); // Console background color

    static ImVec4 GetColorForLogLevel(const spdlog::level::level_enum level)
    {
        switch (level) {
            case spdlog::level::trace:
                return {0.5f, 0.5f, 0.5f, 1.0f}; // Gray
            case spdlog::level::debug:
                return {0.3f, 0.65f, 0.79f, 1.0f}; // Darker cyan
            case spdlog::level::info:
                return {0.34f, 0.72f, 0.50f, 1.0f}; // Light green
            case spdlog::level::warn:
                return {0.9f, 0.9f, 0.3f, 1.0f}; // Yellow
            case spdlog::level::err:
                return {1.0f, 0.3f, 0.3f, 1.0f}; // Red
            case spdlog::level::critical:
                return {1.0f, 0.0f, 1.0f, 1.0f}; // Bright magenta
            default:
                return {1.0f, 1.0f, 1.0f, 1.0f}; // White
        }
    }

    static void TestCommand()
    {
        Log::Trace("This is a Trace message");
        Log::Debug("This is a Debug message");
        Log::Info("This is an Info message");
        Log::Warn("This is a Warning message");
        Log::Error("This is an Error message");
        Log::Fatal("This is a Critical message");
    }

    QuakeConsole::QuakeConsole(bool initiallyVisible)
        : _buffer(std::make_shared<Detail::ConsoleBuffer>(MAX_BUFFER_SIZE))
        , _sink(std::make_shared<Detail::ConsoleSinkMt>(_buffer))
        , _visible(initiallyVisible)
        , _animationProgress(initiallyVisible ? 1.0f : 0.0f)
        , _focusTarget(initiallyVisible ? ConsoleFocus::CommandInput : ConsoleFocus::None)
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
            _focusTarget = ConsoleFocus::CommandInput;
        }
    }

    void QuakeConsole::Render()
    {
        // Don't render if fully hidden
        if (!_visible && _animationProgress <= 0.0f) {
            return;
        }

        const float deltaTime = ImGui::GetIO().DeltaTime;

        // Animate console sliding down/up
        if (_visible && _animationProgress < 1.0f) {
            _animationProgress = std::min(1.0f, _animationProgress + ANIMATION_SPEED * deltaTime);
        } else if (!_visible && _animationProgress > 0.0f) {
            _animationProgress = std::max(0.0f, _animationProgress - ANIMATION_SPEED * deltaTime);
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
        ImGui::SetNextWindowSizeConstraints(ImVec2(windowWidth, CONSOLE_MIN_HEIGHT), ImVec2(windowWidth, viewport->WorkSize.y * CONSOLE_MAX_HEIGHT_RATIO));

        // Console window style
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImGui::GetStyle().ItemSpacing);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, CONSOLE_BG_COLOR);

        const ImGuiWindowFlags windowFlags = 
            ImGuiWindowFlags_NoTitleBar | 
            ImGuiWindowFlags_NoCollapse | 
            ImGuiWindowFlags_NoSavedSettings | 
            ImGuiWindowFlags_NoDocking;

        if (ImGui::Begin("QuakeConsole", nullptr, windowFlags)) {
            RenderFilters();
            RenderLogOutput();
            RenderCommandInput();

            // Set focus to command input when console opens
            if (_focusTarget == ConsoleFocus::CommandInput && _animationProgress > 0.95f) {
                ImGui::SetKeyboardFocusHere(-1);
                _focusTarget = ConsoleFocus::None;
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

    void QuakeConsole::Clear()
    {
        _buffer->Clear();
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
        // Calculate button size based on single character + frame padding
        const auto& style = ImGui::GetStyle();
        const float buttonWidth = ImGui::CalcTextSize("W").x + style.FramePadding.x * 2.0f;
        const ImVec2 buttonSize(buttonWidth, 0.0f);

        // Helper lambda for flat toggle buttons
        auto ToggleButton = [buttonWidth](const char* label, bool* value, spdlog::level::level_enum level, const char* tooltip = nullptr) {
            const ImVec4 levelColor = GetColorForLogLevel(level);

            if (*value) {
                // Active: thick colored border
                ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 2.0f);
                ImGui::PushStyleColor(ImGuiCol_Border, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
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
            const ImVec2 buttonSize(buttonWidth, 0.0f);
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
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
            // Hidden functionality: test all log levels on double-click
            TestCommand();
        }

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
        auto itemSpacing = style.ItemSpacing.x; // additional same as between items
        ImGui::Dummy(ImVec2(itemSpacing, 0));

        // TODO: find good and small font with unicode icons
        //  Clear button ✖
        ImGui::SameLine();
        if (ImGui::Button("≠", buttonSize)) {
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
        if (ImGui::Button("∞", buttonSize)) {
            _autoScroll = !_autoScroll;
        }
        ImGui::PopStyleColor(2);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Auto-scroll");
        }

        ImGui::SameLine();
        ImGui::Dummy(ImVec2(itemSpacing, 0));

        // Text filter input
        ImGui::SameLine();
        const float closeButtonWidth = 20.0f;
        const float availableWidth = ImGui::GetContentRegionAvail().x - closeButtonWidth - itemSpacing - ImGui::GetStyle().ItemSpacing.x;
        ImGui::SetNextItemWidth(availableWidth);
        
        // Handle TAB navigation: set focus if requested
        if (_focusTarget == ConsoleFocus::FilterInput) {
            ImGui::SetKeyboardFocusHere();
            _focusTarget = ConsoleFocus::None;
        }
        
        ImGui::InputTextWithHint("##FilterText", "Search...", _filterText.data(), _filterText.size());
        
        // Handle Shift+TAB to go back to command input
        if (ImGui::IsItemActive() && ImGui::IsKeyPressed(ImGuiKey_Tab) && ImGui::GetIO().KeyShift) {
            _focusTarget = ConsoleFocus::CommandInput;
        }

        ImGui::SameLine();
        ImGui::Dummy(ImVec2(itemSpacing, 0));

        // Close button (right-aligned) - using ImGui::CloseButton
        const float closeButtonSize = ImGui::GetFontSize();
        ImGui::SameLine(ImGui::GetWindowWidth() - closeButtonSize - style.WindowPadding.x);
        
        // Calculate vertical centering offset
        const float frameHeight = ImGui::GetFrameHeight();
        const float yOffset = (frameHeight - closeButtonSize) * 0.5f;
        
        ImGui::AlignTextToFramePadding();
        const ImVec2 closeButtonPos = ImVec2(ImGui::GetCursorScreenPos().x, ImGui::GetCursorScreenPos().y + yOffset);
        const ImGuiID closeButtonId = ImGui::GetID("##CloseButton");
        
        // Reserve space for the button so it doesn't overlap next line
        ImGui::Dummy(ImVec2(closeButtonSize, closeButtonSize));
        
        // Draw close button at the reserved position
        if (ImGui::CloseButton(closeButtonId, closeButtonPos)) {
            Hide();
        }
    }

    void QuakeConsole::RenderLogOutput()
    {
        const float footerHeight = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();

        ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footerHeight), ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);

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

            // Apply text filter (case-insensitive)
            if (_filterText[0] != '\0') {
                const std::string filterStr(_filterText.data());
                const auto found = std::search(entry.message.begin(), entry.message.end(), filterStr.begin(), filterStr.end(), [](char ch1, char ch2) {
                    return std::tolower(static_cast<unsigned char>(ch1)) == std::tolower(static_cast<unsigned char>(ch2));
                });
                if (found == entry.message.end()) {
                    return;
                }
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
            ImGui::SetWindowFontScale(1.0f); // BeginChild starts with scale 1.0f by default
            ImGui::PopFont();
        }

        ImGui::EndChild();
    }

    void QuakeConsole::RenderCommandInput()
    {
        static std::array<char, 256> inputBuf{};
        const ImGuiInputTextFlags inputFlags = ImGuiInputTextFlags_EnterReturnsTrue;

        ImGui::PushItemWidth(-1);
        
        // Handle focus request from TAB navigation
        if (_focusTarget == ConsoleFocus::CommandInput) {
            ImGui::SetKeyboardFocusHere();
            _focusTarget = ConsoleFocus::None;
        }
        
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
        
        // Handle TAB to move to filter input
        if (ImGui::IsItemActive() && ImGui::IsKeyPressed(ImGuiKey_Tab) && !ImGui::GetIO().KeyShift) {
            _focusTarget = ConsoleFocus::FilterInput;
        }
        
        ImGui::PopItemWidth();
    }

    void QuakeConsole::ExecuteCommand(const std::string& command)
    {
        // Log the command
        Log::Info("{}", command);

        // Process commands
        if (command == "clear") {
            Clear();
        } else if (command == "help") {
            Log::Info("Available commands:");
            Log::Info("  help  - Show this help message");
            Log::Info("  clear - Clear console output");
            Log::Info("  test  - Test all log levels");
        } else if (command == "test") {
            TestCommand();
        } else {
            Log::Warn("Unknown command: '{}'. Type 'help' for available commands.", command);
        }
    }

} // namespace Im
