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
    covent::instant_task<void> quick(int &i) {
        ++i;
        co_return;
    }
    covent::task<void> suspend(int & i) {
        ++i;
        co_await quick(i);
        ++i;
        co_return;
    }
}

TEST(Coro, run_suspend) {
    covent::Loop loop;

}