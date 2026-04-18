#pragma once
#include "Fs/Drive.h"
#include "Log/Log.h"
#include "imgui.h"
#include <memory>

struct SDL_Window;
struct SDL_Renderer;
union SDL_Event;

namespace Im
{
    class Deputy
    {
        static Log::Logger _logger;

    public:
        struct Config
        {
            SDL_Window* window;
            SDL_Renderer* renderer;
            std::shared_ptr<Fs::Drive> drive;
        };

        explicit Deputy(Config config);
        ~Deputy();

        void UpdateBegin();
        void UpdateEnd();
        void ProcessSdlEvent(const SDL_Event& event);

        [[nodiscard]] const ImGuiIO& GetImGuiIO() const { return *_io; }
        [[nodiscard]] ImGuiID GetDockSpaceId() const { return _dockSpaceId; }

    private:
        void LoadFonts();
        bool LoadFont(const Fs::Path& fontPath, const char* fontName, float fontSize);

        SDL_Window* _window;
        SDL_Renderer* _renderer;
        std::shared_ptr<Fs::Drive> _drive;

        ImGuiContext* _context{};
        ImGuiIO* _io{};
        std::vector<std::vector<uint8_t>> _fontData;

        // Update
        ImGuiID _dockSpaceId{};
    };
}
