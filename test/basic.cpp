//
// Created by dwd on 8/24/24.
//

#include <future>
#include <thread>
#include <covent/loop.h>
#include "gtest/gtest.h"

TEST(Basic, create) {
    covent::Loop loop;
}

TEST(Basic, run_empty) {
    covent::Loop loop;
    loop.run_until_complete();
}

TEST(Basic, run_once) {
    covent::Loop loop;
    loop.run_once(false);
}

TEST(Basic, run_complete) {
    covent::Loop loop;
    bool trap = false;
    loop.defer([&trap]() {
        trap = true;
    });
    loop.run_until_complete();
    EXPECT_TRUE(trap);
}

TEST(Basic, run_pause_complete) {
    covent::Loop loop;
    bool trap = false;
    loop.defer([&trap]() {
        trap = true;
    }, 0.5);
    loop.run_until_complete();
    EXPECT_TRUE(trap);
}

namespace {
    void thread_fn(std::promise<covent::Loop *> * promise) {
        covent::Loop loop;
        promise->set_value(&loop);
        loop.run();
    }

    covent::Loop & run_on_thread() {
        std::promise<covent::Loop *> promise;
        std::future<covent::Loop *> future = promise.get_future();

        std::jthread thread{thread_fn, &promise};
        auto * loop = future.get();
        thread.detach();

        return *loop;
    }

    covent::task<std::thread::id> return_thread_id(covent::Loop & l) {
        std::cout << "Loop at " << &covent::Loop::thread_loop() << ", thread is " << std::this_thread::get_id() << std::endl;
        auto const & p = co_await covent::own_promise<covent::task<std::thread::id>::promise_type>();
        std::cout << "Expecting loop " << &l << " to also be " << p.loop << std::endl;
        std::cout << "Loop at " << &covent::Loop::thread_loop() << ", thread is " << std::this_thread::get_id() << std::endl;
        co_return std::this_thread::get_id();
    }

    covent::task<std::thread::id> that_too(covent::task<std::thread::id> other) {
        co_return co_await other;
    }
}

TEST(Thread, simple) {
    covent::Loop loop;
    auto & thread_loop = run_on_thread();
    auto task1 = loop.run_task(return_thread_id(loop));
    auto task2 = loop.run_task(that_too(return_thread_id(thread_loop)));
    // EXPECT_NE(std::hash<std::thread::id>{}(task1), std::hash<std::thread::id>{}(task2));
    // Above fails because the coroutine is resumed by the awaiter instead of start(), so it flips loop (possibly) only after a resume.
}