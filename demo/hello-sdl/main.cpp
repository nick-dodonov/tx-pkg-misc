#include "App/Domain.h"
#include "FpsCounter.h"
#include "Log/Log.h"
#include "Sdl/Loop/Sdl3Runner.h"
#include <boost/asio/experimental/awaitable_operators.hpp>

namespace
{
    static constexpr int DefaultTimeoutSeconds = 2;
    std::shared_ptr<App::Domain> domain;
}

[[maybe_unused]] static boost::asio::awaitable<int> CoroMain(std::shared_ptr<Sdl::Loop::Sdl3Runner> runner, int timeoutSeconds)
{
    if (timeoutSeconds <= 0) {
        Log::Info("WAITING: stop signal...");
        co_await domain->AsyncStopped();
        Log::Info("EXITING: stop signal received");
    } else {
        namespace asio = boost::asio;
        using namespace asio::experimental::awaitable_operators;

        auto executor = co_await asio::this_coro::executor;

        // Wait for EITHER quit event OR timer - whichever comes first
        Log::Info("WAITING: quit event or timeout ({} seconds)...", timeoutSeconds);

        auto timer = asio::steady_timer(executor);
        timer.expires_after(std::chrono::seconds(timeoutSeconds));
        auto result = co_await (domain->AsyncStopped() || timer.async_wait(asio::as_tuple(asio::use_awaitable)));

        if (result.index() == 0) {
            auto ec = std::get<0>(result);
            Log::Info("EXITING: window was closed by user: {}", ec ? ec.what() : "<success>");
        } else {
            auto [ec] = std::get<1>(result);
            Log::Info("EXITING: timeout is reached: {}", ec ? ec.what() : "<success>");
        }
    }
    co_return timeoutSeconds;
}

struct MyHandler
    : App::Loop::Handler
    , Sdl::Loop::Sdl3Handler
{
    void Update(const App::Loop::UpdateCtx& ctx) override
    {
        auto* renderer = (dynamic_cast<Sdl::Loop::Sdl3Runner&>(ctx.Runner)).GetRenderer();

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
        SDL_FRect rect = {x - size / 2, y - size / 2, size, size};
        SDL_RenderFillRect(renderer, &rect);

        // Second rectangle rotating opposite direction
        float x2 = centerX + radius * std::cosf(-elapsed * 1.5f);
        float y2 = centerY + radius * std::sinf(-elapsed * 1.5f);
        SDL_SetRenderDrawColor(renderer, 100, 200, 100, 255);
        SDL_FRect rect2 = {x2 - 25, y2 - 25, 50, 50};
        SDL_RenderFillRect(renderer, &rect2);

        // Render debug text with session and frame info
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderDebugTextFormat(renderer, 10.0f, 10.0f, "Session Time: %.2f s", ctx.session.passedSeconds);
        SDL_RenderDebugTextFormat(renderer, 10.0f, 20.0f, "Frame Index: %llu", static_cast<unsigned long long>(ctx.frame.index));
        SDL_RenderDebugTextFormat(renderer, 10.0f, 30.0f, "Delta: %.2f ms", ctx.frame.deltaSeconds * 1000.0f);
        SDL_RenderDebugTextFormat(renderer, 10.0f, 40.0f, "Avg FPS: %.1f", fpsCounter.GetAverageFps());
    }

    SDL_AppResult Sdl3Event(Sdl::Loop::Sdl3Runner& runner, const SDL_Event& event) override
    {
        if (event.type == SDL_EVENT_QUIT) {
            Log::Info("received SDL_EVENT_QUIT");
            return SDL_APP_SUCCESS;
        }
        if (event.type == SDL_EVENT_KEY_DOWN) {
            if (event.key.key == SDLK_ESCAPE) {
                Log::Info("ESC({}) pressed, quitting by event", static_cast<int>(event.key.key));
                return SDL_APP_SUCCESS;
            } else {
                Log::Trace("Key({}) pressed", static_cast<int>(event.key.key));
            }
        }
        return SDL_APP_CONTINUE;
    }
};

int main(const int argc, const char* argv[])
{
    // Get timeout from first argument
    Boot::CliArgs args(argc, argv);
    auto timeoutSeconds = args.GetIntArg(1).value_or(DefaultTimeoutSeconds);

    // Configure SDL3 runner
    auto composite = std::make_shared<App::Loop::CompositeHandler>();
    auto handler = std::make_shared<MyHandler>();
    composite->Add(*handler);
    auto runner = std::make_shared<Sdl::Loop::Sdl3Runner>(
        composite,
        handler,
        Sdl::Loop::Sdl3Runner::Options{
            .Window = {
                .Title = "Hello SDL3",
                .Width = 640,
                .Height = 480,
            },
        }
    );

    // Create domain with custom runner
    domain = std::make_shared<App::Domain>(args);
    composite->Add(*domain);
    return domain->RunCoroMain(runner, CoroMain(runner, timeoutSeconds));
}
