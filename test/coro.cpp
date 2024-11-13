//
// Created by dwd on 8/24/24.
//

#include <covent/loop.h>
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
    EXPECT_EQ(i, 11);
    EXPECT_FALSE(task.done());
    loop.run_until_complete();
    EXPECT_EQ(i, 12);
    EXPECT_FALSE(task.done());
    run_suspend::fut_slow.resolve(20);
    loop.run_until_complete();
    EXPECT_EQ(i, 33);
    EXPECT_TRUE(task.done());
}