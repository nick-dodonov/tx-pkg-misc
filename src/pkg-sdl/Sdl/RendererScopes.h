#pragma once
#include <SDL3/SDL_render.h>

namespace Sdl
{
    struct SetRenderScaleScope
    {
        SDL_Renderer* renderer;
        float previous_x{}, previous_y{};

        SetRenderScaleScope(SDL_Renderer* renderer, float x, float y)
            : renderer(renderer)
        {
            SDL_GetRenderScale(renderer, &previous_x, &previous_y);
            SDL_SetRenderScale(renderer, x, y);
        }

        ~SetRenderScaleScope() { SDL_SetRenderScale(renderer, previous_x, previous_y); }
    };

}
