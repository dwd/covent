//
// Created by dwd on 12/27/24.
//

#include <covent/app.h>
#include <gtest/gtest.h>

GTEST_TEST(App, def) {
    auto const & app = covent::Application::application();
    EXPECT_NE(app.name(), "");
    delete &app; // Test hackery
}

GTEST_TEST(App, set) {
    covent::Application my_app{"test"};
    auto const & app = covent::Application::application();
    EXPECT_EQ(app.name(), "test");
}

GTEST_TEST(App, singleton) {
    covent::Application my_app{"test3"};
    EXPECT_ANY_THROW(
        covent::Application second_app{"test2"};
    );
    auto const & app = covent::Application::application();
    EXPECT_EQ(app.name(), "test3");
}
