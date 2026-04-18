#include "DemoHandler.h"

#include "Boot/Boot.h"
#include "RunLoop/CompositeHandler.h"
#include "Sdl/Loop/Sdl3Runner.h"

int main(const int argc, const char* argv[])
{
    Boot::DefaultInit(argc, argv);

    auto composite = std::make_shared<RunLoop::CompositeHandler>();
    auto handler = std::make_shared<Demo::DemoHandler>(*composite);
    composite->Add(*handler);

    auto runner = std::make_shared<Sdl::Loop::Sdl3Runner>(
        composite,
        handler,
        Sdl::Loop::Sdl3Runner::Options{
            .Window = {
                .Title = "Peer Mix Demo",
                .Width = 1200,
                .Height = 800,
                .Flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_FILL_DOCUMENT,
            },
        }
    );
    return runner->Run();
}
