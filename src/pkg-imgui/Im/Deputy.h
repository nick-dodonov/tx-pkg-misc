#pragma once
#include "imgui.h"

struct SDL_Window;
struct SDL_Renderer;
union SDL_Event;

namespace Im
{
    class Deputy
    {
    public:
        Deputy(SDL_Window* window, SDL_Renderer* renderer);
        ~Deputy();

        void UpdateBegin();
        void UpdateEnd();
        void ProcessSdlEvent(const SDL_Event& event);

        [[nodiscard]] const ImGuiIO& GetImGuiIO() const { return *_imGuiIO; }

    private:
        SDL_Window* _window = nullptr;
        SDL_Renderer* _renderer = nullptr;
        ImGuiIO* _imGuiIO = nullptr;
    };
}
