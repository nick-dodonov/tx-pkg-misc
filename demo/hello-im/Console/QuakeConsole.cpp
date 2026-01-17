#include "QuakeConsole.h"
#include "imgui.h"
#include <spdlog/spdlog.h>
#include <array>
#include <string>

namespace Im
{
    QuakeConsole::QuakeConsole(bool initiallyVisible)
        : _buffer(std::make_shared<Detail::ConsoleBuffer>(1000))
        , _sink(std::make_shared<Detail::ConsoleSinkMt>(_buffer))
        , _visible(initiallyVisible)
        , _animationProgress(initiallyVisible ? 1.0f : 0.0f)
        , _shouldFocusInput(initiallyVisible)
    {
    }

    QuakeConsole::~QuakeConsole()
    {
        // Remove sink from spdlog
        auto logger = spdlog::default_logger();
        if (logger) {
            auto& sinks = logger->sinks();
            sinks.erase(
                std::remove(sinks.begin(), sinks.end(), _sink),
                sinks.end()
            );
        }
    }

    void QuakeConsole::Initialize()
    {
        // Add our sink to the default logger
        auto logger = spdlog::default_logger();
        if (logger) {
            logger->sinks().push_back(_sink);
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
        
        const ImGuiWindowFlags windowFlags = 
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoSavedSettings;
        
        if (ImGui::Begin("QuakeConsole", nullptr, windowFlags)) {
            // Console header
            ImGui::Text("Console");
            ImGui::SameLine();
            
            // Filter buttons
            ImGui::SameLine(windowWidth - 250);
            if (ImGui::SmallButton("Clear")) {
                Clear();
            }
            ImGui::SameLine();
            ImGui::Checkbox("Auto-scroll", &_autoScroll);
            
            ImGui::Separator();
            
            // Log output area
            const float footerHeight = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
            ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footerHeight), ImGuiChildFlags_Borders, ImGuiWindowFlags_None);
            
            // Display log entries
            _buffer->ForEach([](const Detail::ConsoleBuffer::LogEntry& entry) {
                // Color based on log level
                ImVec4 color;
                switch (entry.level) {
                    case spdlog::level::trace:
                        color = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);  // Gray
                        break;
                    case spdlog::level::debug:
                        color = ImVec4(0.4f, 0.8f, 1.0f, 1.0f);  // Cyan
                        break;
                    case spdlog::level::info:
                        color = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);  // Light gray
                        break;
                    case spdlog::level::warn:
                        color = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);  // Yellow
                        break;
                    case spdlog::level::err:
                        color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);  // Red
                        break;
                    case spdlog::level::critical:
                        color = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);  // Bright red
                        break;
                    default:
                        color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);  // White
                        break;
                }
                
                ImGui::PushStyleColor(ImGuiCol_Text, color);
                ImGui::TextUnformatted(entry.message.c_str());
                ImGui::PopStyleColor();
            });
            
            // Auto-scroll to bottom
            if (_autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
                ImGui::SetScrollHereY(1.0f);
            }
            
            ImGui::EndChild();
            
            // Command input area
            ImGui::Separator();
            static std::array<char, 256> inputBuf{};
            const ImGuiInputTextFlags inputFlags = 
                ImGuiInputTextFlags_EnterReturnsTrue;
            
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
        spdlog::info("> {}", command);
        
        // Process commands
        if (command == "clear") {
            Clear();
        }
        else if (command == "help") {
            spdlog::info("Available commands:");
            spdlog::info("  help  - Show this help message");
            spdlog::info("  clear - Clear console output");
            spdlog::info("  test  - Test all log levels");
        }
        else if (command == "test") {
            spdlog::trace("[TEST] This is a TRACE message");
            spdlog::debug("[TEST] This is a DEBUG message");
            spdlog::info("[TEST] This is an INFO message");
            spdlog::warn("[TEST] This is a WARNING message");
            spdlog::error("[TEST] This is an ERROR message");
            spdlog::critical("[TEST] This is a CRITICAL message");
        }
        else {
            spdlog::warn("Unknown command: '{}'. Type 'help' for available commands.", command);
        }
    }

} // namespace Im
