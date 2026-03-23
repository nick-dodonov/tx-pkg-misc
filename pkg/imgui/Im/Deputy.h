#pragma once
#include "imgui.h"
#include "Log/Log.h"
#include "Fs/Drive.h"

struct SDL_Window;
struct SDL_Renderer;
union SDL_Event;

namespace Im
{
    class Deputy
    {
        static Log::Logger _logger;

    public:
        Deputy(SDL_Window* window, SDL_Renderer* renderer);
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

        ImGuiContext* _context{};
        ImGuiIO* _io{};
        std::vector<std::vector<uint8_t>> _fontData;

        // Update
        ImGuiID _dockSpaceId{};
    };
}
