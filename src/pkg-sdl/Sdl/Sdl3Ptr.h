#pragma once
#include <SDL3/SDL.h>
#include <memory>

namespace Sdl
{
    template <typename T, void (*TDeleter)(T*)>
    struct Deleter
    {
        void operator()(T* ptr) const noexcept
        {
            if (ptr) {
                TDeleter(ptr);
            }
        }
    };

    template <typename T, void (*TDeleter)(T*)>
    using UniquePtr = std::unique_ptr<T, Deleter<T, TDeleter>>;

    using Window = UniquePtr<SDL_Window, SDL_DestroyWindow>;
    using Renderer = UniquePtr<SDL_Renderer, SDL_DestroyRenderer>;
}
