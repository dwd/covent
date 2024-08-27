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
    EXPECT_TRUE(trap);
}