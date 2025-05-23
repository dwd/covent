//
// Created by dwd on 8/24/24.
//

#include <covent/loop.h>
#include <covent/gather.h>
#include <covent/sleep.h>

#include "covent/generator.h"
#include "gtest/gtest.h"

namespace run_single {
    covent::task<void> empty(bool & b) {
        std::cout << "Hello" << std::endl;
        b = true;
        co_return;
    }
}

TEST(Coro, run_single) {
    covent::Loop loop;
    bool trap = false;
    auto task = run_single::empty(trap);
    EXPECT_FALSE(trap);
    auto r1 = task.start();
    EXPECT_TRUE(trap);
    EXPECT_TRUE(r1);
    EXPECT_TRUE(task.done());
}

namespace run_suspend {
    covent::future<int> fut_quick;
    covent::instant_task<void> quick(int &i) {
        i += co_await fut_quick;
        co_return;
    }
    covent::future<int> fut_slow;
    covent::task<void> nested() {
        co_return;
    }
    covent::task<void> slow(int &i) {
        i += co_await fut_slow;
        co_await nested();
        co_return;
    }
    covent::task<void> suspend(int & i) {
        ++i;
        co_await quick(i);
        ++i;
        co_await slow(i);
        ++i;
        co_return;
    }
}

TEST(Coro, run_suspend) {
    covent::Loop loop;
    int i = 0;
    auto task = run_suspend::suspend(i);
    auto r1 = task.start();
    EXPECT_EQ(i, 1);
    EXPECT_FALSE(r1);
    EXPECT_FALSE(task.done());
    loop.run_until_complete();
    EXPECT_EQ(i, 1);
    EXPECT_FALSE(task.done());
    run_suspend::fut_quick.resolve(10);
    EXPECT_EQ(i, 12);
    EXPECT_FALSE(task.done());
    loop.run_until_complete();
    EXPECT_EQ(i, 12);
    EXPECT_FALSE(task.done());
    run_suspend::fut_slow.resolve(20);
    loop.run_until_complete();
    EXPECT_EQ(i, 33);
    EXPECT_TRUE(task.done());
}

namespace {
    covent::instant_task<void *> my_promise() {
        auto * promise = &co_await covent::own_promise<covent::instant_task<void *>::promise_type>();
        co_return static_cast<void *>(promise);
    }
    covent::task<void *> my_promise2() {
        auto * promise = &co_await covent::own_promise<covent::task<void *>::promise_type>();
        co_return static_cast<void *>(promise);
    }
}

TEST(Coro, introspection) {
    auto task = my_promise();
    task.start();
    EXPECT_EQ(task.done(), true);
    auto ret = task.get();
    EXPECT_EQ(ret, &task.handle.promise());
}

namespace {
    sigslot::signal<int> sigslot_signal;
    sigslot::signal<std::string &> sigslot_signal_str;
    covent::instant_task<int> sigslot_test_instant() {
        co_return co_await sigslot_signal;
    }
    covent::task<int> sigslot_test() {
        co_return co_await sigslot_signal;
    }
    covent::task<std::string&> sigslot_test_str() {
        auto & ref = co_await sigslot_signal_str;
        co_return ref;
    }
}

TEST(CoroSigslot, sigslot_test_instant) {
    auto task = sigslot_test_instant();
    auto t1 = task.start();
    EXPECT_FALSE(t1);
    sigslot_signal(42);
    EXPECT_TRUE(task.done());
    EXPECT_EQ(task.get(), 42);
}

TEST(CoroSigslot, sigslot_test) {
    covent::Loop loop;
    auto task = sigslot_test();
    auto t1 = task.start();
    EXPECT_FALSE(t1);
    sigslot_signal(42);
    loop.run_until_complete();
    EXPECT_TRUE(task.done());
    EXPECT_EQ(task.get(), 42);
}

TEST(CoroSigslot, sigslot_test_str) {
    covent::Loop loop;
    bool completed = false;
    auto task = sigslot_test_str();
    auto handle = task.on_completed([&completed]() {
        completed = true;
    });
    auto t1 = task.start();
    EXPECT_FALSE(t1);
    EXPECT_FALSE(completed);
    std::string test_result = "Test result";
    sigslot_signal_str(test_result);
    loop.run_until_complete();
    EXPECT_TRUE(task.done());
    EXPECT_TRUE(completed);
    auto & result = task.get();
    EXPECT_EQ(result, "Test result");
    EXPECT_EQ(&result, &test_result);
}

namespace {
    covent::task<int> first() {
        co_await covent::sleep(0.1);
        co_return 1;
    }
    covent::task<long> second() {
        co_return 1 + co_await first();
    }
}

TEST(CoroGather, single) {
    covent::Loop loop;
    auto result = loop.run_task(second());
    EXPECT_EQ(result, 2);
}

TEST(CoroGather, gather) {
    covent::Loop loop;
    auto task = covent::gather(first(), second());
    task.start();
    while (!task.done()) loop.run_until_complete();
    auto [one, two] = task.get();
    EXPECT_EQ(one, 1);
    EXPECT_EQ(two, 2);
}

namespace {
    covent::task<int> winner() {
        co_return 1;
    }
    covent::task<int> hare() {
        co_await covent::sleep(0.3);
        co_return 2;
    }
    covent::task<int> tortoise() {
        co_await covent::sleep(0.1);
        co_await covent::sleep(0.1);
        co_return 3;
    }
}

TEST(CoroRace, race_instant_single) {
    covent::Loop loop;
    auto ret= loop.run_task(covent::race(
        winner()
    ));
    EXPECT_EQ(ret, 1);
}

TEST(CoroRace, race_sleep_single) {
    covent::Loop loop;
    auto ret= loop.run_task(covent::race(
        tortoise()
    ));
    EXPECT_EQ(ret, 3);
}

TEST(CoroRace, race_all) {
    covent::Loop loop;
    auto ret= loop.run_task(covent::race(
        winner(),
        hare(),
        tortoise()
    ));
    EXPECT_EQ(ret, 1);
}

TEST(CoroRace, race_classic) {
    covent::Loop loop;
    auto ret= loop.run_task(covent::race(
        hare(),
        tortoise()
    ));
    EXPECT_EQ(ret, 3);
}

namespace {
    covent::task<void> inner() {
        auto span = covent::sentry::span::start("fn.call", "inner");
        co_await covent::own_promise<covent::task<void>::promise_type>();
        throw std::runtime_error("Whoops");
    }
    covent::task<void> outer() {
        auto trans = covent::sentry::transaction::start("test.test", "outer");
        auto span = covent::sentry::span::start("fn.call", "outer");
        co_await inner();
    }
}

TEST(CoroThrow, single) {
    covent::Loop loop;
    auto task = inner();
    task.start();
    EXPECT_TRUE(task.done());
    EXPECT_ANY_THROW(task.get());
}

TEST(CoroThrow, nested) {
    covent::Loop loop;
    auto task = outer();
    task.start();
    loop.run_until_complete();
    EXPECT_TRUE(task.done());
    EXPECT_ANY_THROW(task.get());
}

TEST(CoroThrow, nested_run_task) {
    covent::Loop loop;
    EXPECT_ANY_THROW(loop.run_task(outer()));
}

namespace {
    covent::task<std::unique_ptr<int>> unique_ptr_int(int x) {
        co_return std::make_unique<int>(x);
    }
    covent::task<std::string> std_string(const char * x) {
        co_return x;
    }
}

TEST(CoroReturn, movable) {
    covent::Loop loop;
    EXPECT_EQ(*loop.run_task(unique_ptr_int(42)), 42);
}
TEST(CoroReturn, copy) {
    covent::Loop loop;
    EXPECT_EQ(loop.run_task(std_string("42")), "42");
}

namespace {
    covent::generator<int> counter() {
        for (int i = 0; i != 10; ++i) {
            co_yield i;
        }
    }

    covent::generator_async<int> counter_async() {
        for (int i = 0; i != 10; ++i) {
            std::cout << "1 Counter is " << i << std::endl;
            co_await covent::sleep(0.1);
            std::cout << "2 Counter is " << i << std::endl;
            co_yield i;
            std::cout << "3 Counter is " << i << std::endl;
        }
    }

    covent::task<int> counter_check() {
        int i = 0;
        auto gen = counter_async();
        for (auto j = co_await gen.begin(); j != gen.end(); co_await ++j) {
            std::cout << "X Counter is " << *j << std::endl;
            co_await covent::sleep(0.1);
            i += *j;
        }
        co_return i;
    }
}

TEST(Generator, simple) {
    int j = 0;
    for (auto i : counter()) {
        EXPECT_EQ(i, j);
        EXPECT_NE(i, 10);
        ++j;
    }
    EXPECT_EQ(j, 10);
}

TEST(Generator, async) {
    covent::Loop loop;
    auto r = loop.run_task(counter_check());
    EXPECT_EQ(r, 45);
}