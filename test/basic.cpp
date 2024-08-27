//
// Created by dwd on 8/24/24.
//

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
