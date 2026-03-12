#pragma once
#include "imgui.h"
#include "Log/Log.h"

struct SDL_Window;
struct SDL_Renderer;
union SDL_Event;

namespace std::filesystem {
    class path;
}

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
        bool AddFontFromFileTTF(const std::filesystem::path& path, float size_pixels = 0.0f);

        SDL_Window* _window;
        SDL_Renderer* _renderer;

        ImGuiContext* _context{};
        ImGuiIO* _io{};

        // Update
        ImGuiID _dockSpaceId{};
    };
}
