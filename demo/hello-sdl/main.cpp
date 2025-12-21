#include "App/Domain.h"
#include "Log/Log.h"
#include <memory>

static std::shared_ptr<App::Domain> domain;

static boost::asio::awaitable<int> CoroMain()
{
    auto executor = co_await boost::asio::this_coro::executor;
    Log::Info("CoroMain running on executor type: {}", typeid(executor).name());
    
    for (auto i = 0; i < 5; ++i) {
        Log::Info("CoroMain tick {}", i);
        co_await boost::asio::post(executor, boost::asio::use_awaitable);
    }

    co_return 0;
}

int main(const int argc, const char* argv[])
{
    domain = std::make_shared<App::Domain>(argc, argv);
    return domain->RunCoroMain(CoroMain());
}
