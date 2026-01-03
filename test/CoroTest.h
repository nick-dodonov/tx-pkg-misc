#pragma once
#include "CoroSyn.h"
#include "Log/Log.h"
#include <gtest/gtest.h>

struct CoroTest : testing::Test {
    void SetUp() override { 
        const auto* test_name = ::testing::UnitTest::GetInstance()->current_test_info()->name();
        Log::Debug("CoroTest: {}: SetUp", test_name);
    }

    void TearDown() override {
        const auto* test_name = ::testing::UnitTest::GetInstance()->current_test_info()->name();
        Log::Debug("CoroTest: {}: >>>>", test_name);
        while (!coroutineCompleted) {
            if (!synCtx.run_once()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
        ASSERT_TRUE(synCtx.empty());
        Log::Debug("CoroTest: {}: <<<<", test_name);
    }

    QueueSynCtx synCtx;
    bool coroutineCompleted = {};

    struct Promise {
        CoroTest& testInstance;

        /// Constructor automatically receives reference to CoroTest object
        Promise(CoroTest& test) noexcept : testInstance(test) {}
        /// The coroutine's return object is void for test functions
        void get_return_object() noexcept {}

        struct InitialSuspend {
            bool await_ready() noexcept { return false; }
            void await_suspend(std::coroutine_handle<Promise> handle) noexcept { 
                handle.promise().testInstance.synCtx.post(handle);
            }
            void await_resume() noexcept {}
        };

        InitialSuspend initial_suspend() noexcept { return {}; }

        struct FinalAwaiter {
            bool await_ready() noexcept { return false; }
            void await_suspend(std::coroutine_handle<Promise> handle) noexcept { 
                handle.promise().testInstance.coroutineCompleted = true;
            }
            void await_resume() noexcept {}
        };

        FinalAwaiter final_suspend() noexcept { return {}; }

        void return_void() noexcept {}
        void unhandled_exception() noexcept {
            Log::Fatal("CoreTest: unhandled exception");
            std::terminate();
        }
    };
};

namespace std {
    template <typename T>
    requires std::is_base_of_v<CoroTest, T>
    struct coroutine_traits<void, T&> {
        using promise_type = CoroTest::Promise;
    };
}
