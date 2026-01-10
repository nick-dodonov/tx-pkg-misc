#include "Sdl/Loop/Sdl3Runner.h"
#include "App/Domain.h"
#include "Log/Log.h"
#include "Boot/CliArgs.h"
#include "FpsCounter.h"
#include <cmath>
#include <memory>
#include <boost/asio/experimental/awaitable_operators.hpp>

namespace asio = boost::asio;
using namespace asio::experimental::awaitable_operators;

static std::shared_ptr<App::Domain> domain;

[[maybe_unused]] static asio::awaitable<int> CoroMain(std::shared_ptr<Sdl::Loop::Sdl3Runner> runner, int timeoutSeconds)
{
    auto executor = co_await asio::this_coro::executor;

    // Wait for EITHER quit event OR timer - whichever comes first
    Log::Info("waiting for quit event or timeout ({} sec)...", timeoutSeconds);
    auto timer = asio::steady_timer(executor);
    timer.expires_after(std::chrono::seconds(timeoutSeconds));
    auto result = co_await (
        runner->WaitQuit() || 
        timer.async_wait(asio::use_awaitable)
    );

    // result.index() tells us which completed first:
    // 0 = quit event, 1 = timer
    if (result.index() == 0) {
        Log::Info("exiting: window was closed by user");
    } else {
        Log::Info("exiting: timeout ({} sec) reached", timeoutSeconds);
    }

    co_return 0;
}

struct MyHandler 
    : App::Loop::IHandler
    , Sdl::Loop::Sdl3Handler
{
    bool Update(App::Loop::IRunner& runner, const App::Loop::UpdateCtx& ctx) override
    {
        auto* renderer = (dynamic_cast<Sdl::Loop::Sdl3Runner&>(runner)).GetRenderer();

        // FPS counter
        static FpsCounter fpsCounter;
        fpsCounter.AddFrame(ctx.frame.deltaSeconds);

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

        // Render debug text with session and frame info
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderDebugTextFormat(renderer, 10.0f, 10.0f,
            "Session Time: %.2f s", ctx.session.passedSeconds);
        SDL_RenderDebugTextFormat(renderer, 10.0f, 20.0f,
            "Frame Index: %llu", static_cast<unsigned long long>(ctx.frame.index));
        SDL_RenderDebugTextFormat(renderer, 10.0f, 30.0f,
            "Delta: %.2f ms", ctx.frame.deltaSeconds * 1000.0f);
        SDL_RenderDebugTextFormat(renderer, 10.0f, 40.0f,
            "Avg FPS: %.1f", fpsCounter.GetAverageFps());

        return true;
    }

    SDL_AppResult Sdl3Event(Sdl::Loop::Sdl3Runner& runner, const SDL_Event& event) override
    {
        if (event.type == SDL_EVENT_QUIT) {
            Log::Debug("received SDL_EVENT_QUIT");
            return SDL_APP_SUCCESS;
        }
        if (event.type == SDL_EVENT_KEY_DOWN) {
            Log::Trace("Key pressed: {}", static_cast<int>(event.key.key));
            if (event.key.key == SDLK_ESCAPE) {
                Log::Debug("ESC pressed, quitting");
                return SDL_APP_SUCCESS;
            }
        }
        return SDL_APP_CONTINUE;
    }
};

int main(const int argc, const char* argv[])
{
    // Get timeout from first argument (default 3 seconds)
    Boot::CliArgs args(argc, argv);
    //auto timeoutResult = args.GetIntArg(1).value_or(3);

    // Configure SDL3 runner
    auto handler = std::make_shared<MyHandler>();
    auto runner = std::make_shared<Sdl::Loop::Sdl3Runner>(
        handler,
        handler,
        Sdl::Loop::Sdl3Runner::Options{
            .Window = {
                .Title = "Hello SDL3",
                .Width = 640,
                .Height = 480,
            },
        }
    );

    runner->Start();

    // // Create domain with custom runner
    // domain = std::make_shared<App::Domain>(argc, argv, runner);
    // return domain->RunCoroMain(runner, CoroMain(timeoutSeconds));
}
