#include "Sdl/Loop/Sdl3Looper.h"
#include "App/Domain.h"
#include "Log/Log.h"
#include <cmath>
#include <memory>
#include <boost/asio/experimental/awaitable_operators.hpp>

namespace asio = boost::asio;
using namespace asio::experimental::awaitable_operators;

static std::shared_ptr<App::Domain> domain;

static asio::awaitable<int> CoroMain()
{
    auto executor = co_await asio::this_coro::executor;
    auto looper = domain->GetLooper<Sdl::Loop::Sdl3Looper>();

    Log::Info("SDL3 window is ready");
    Log::Info("waiting for quit event or 3-second timeout...");

    // Create a timer for 3 seconds
    auto timer = asio::steady_timer(executor);
    timer.expires_after(std::chrono::seconds(3));

    // Wait for EITHER quit event OR timer - whichever comes first
    // Using asio::experimental::awaitable_operators::operator||
    auto result = co_await (
        looper->WaitForQuit() || 
        timer.async_wait(asio::use_awaitable)
    );

    // result.index() tells us which completed first:
    // 0 = quit event, 1 = timer
    if (result.index() == 0) {
        Log::Info("window was closed by user");
    } else {
        Log::Info("3-second timeout reached, requesting quit");
        looper->RequestQuit(0);
    }

    Log::Info("exiting");
    co_return 0;
}

int main(const int argc, const char* argv[])
{
    // Configure SDL3 looper
    auto looper = std::make_shared<Sdl::Loop::Sdl3Looper>(
        Sdl::Loop::Sdl3Looper::Options{
            .Window = {
                .Title = "Hello SDL3",
                .Width = 640,
                .Height = 480,
            },
            .OnRender = [](SDL_Renderer* renderer, const Sdl::Loop::UpdateCtx& ctx) {
                // Accumulate elapsed time from frame deltas
                auto elapsed = ctx.session.passedSeconds;

                // Clear with dark blue
                SDL_SetRenderDrawColor(renderer, 30, 30, 130, 255);
                SDL_RenderClear(renderer);

                // Animated rectangle - moves in circle and pulses
                float centerX = 320.0f;
                float centerY = 240.0f;
                float radius = 100.0f;
                float x = centerX + radius * std::cosf(elapsed * 2.0f);
                float y = centerY + radius * std::sinf(elapsed * 2.0f);
                float size = 50.0f + 20.0f * std::sinf(elapsed * 4.0f);

                // Color cycles through red/orange
                auto r = static_cast<Uint8>(200 + 55 * std::sinf(elapsed * 3.0f));
                auto g = static_cast<Uint8>(80 + 40 * std::sinf(elapsed * 2.0f));
                
                SDL_SetRenderDrawColor(renderer, r, g, 50, 255);
                SDL_FRect rect = {x - size/2, y - size/2, size, size};
                SDL_RenderFillRect(renderer, &rect);

                // Second rectangle rotating opposite direction
                float x2 = centerX + radius * std::cosf(-elapsed * 1.5f);
                float y2 = centerY + radius * std::sinf(-elapsed * 1.5f);
                SDL_SetRenderDrawColor(renderer, 100, 200, 100, 255);
                SDL_FRect rect2 = {x2 - 25, y2 - 25, 50, 50};
                SDL_RenderFillRect(renderer, &rect2);
            },
            .OnEvent = [](const SDL_Event& event) {
                if (event.type == SDL_EVENT_KEY_DOWN) {
                    Log::Debug("Key pressed: {}", static_cast<int>(event.key.key));
                }
            },
        }
    );

    // Create domain with custom looper
    domain = std::make_shared<App::Domain>(argc, argv, looper);
    return domain->RunCoroMain(CoroMain());
}
