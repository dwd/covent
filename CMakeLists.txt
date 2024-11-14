cmake_minimum_required(VERSION 3.28)
project(covent)
option(COVENT_BUILD_TESTS "Build the Covent tests" OFF)
option(COVENT_SENTRY "Use Sentry (also for tests)" OFF)
option(COVENT_COVERAGE "Coverage support on library and tests" OFF)

set(CMAKE_CXX_STANDARD 23)

if (COVENT_SENTRY)
    find_package(sentry)
endif()

find_package(Libevent)
find_package(SigSlot)
find_package(unbound)

add_library(covent SHARED src/covent.cpp include/covent/covent.h
        include/covent/base.h
        include/covent/coroutine.h
        include/covent/loop.h
        include/covent/core.h
        include/covent/listener.h
        src/session.cpp
        src/listener.cpp
        include/covent/sockaddr-cast.h
        include/covent/exceptions.h
        include/covent/http.h
        src/http/fieldref.cpp
        src/http/request.cpp
        include/covent/future.h
        src/http/response.cpp
        src/dns/dns.cc)
target_include_directories(covent PUBLIC include)
target_link_libraries(covent PUBLIC libevent::libevent sigslot::sigslot unbound::unbound)

if(COVENT_COVERAGE)
    target_link_options(covent PUBLIC --coverage)
    target_compile_options(covent PUBLIC --coverage)
endif()

# Tests
if(COVENT_BUILD_TESTS)
    find_package(GTest)
    enable_testing()

    add_executable(covent-test
            test/basic.cpp
            test/main.cpp
            test/coro.cpp
            test/echo.cpp
            test/http.cpp
            test/dns-utils.cc
            test/resolver.cc
    )
    target_include_directories(covent-test SYSTEM PUBLIC include)
    target_link_libraries(covent-test PUBLIC covent GTest::gtest)
    if(COVENT_COVERAGE)
        target_link_options(covent-test PUBLIC --coverage)
        target_compile_options(covent-test PUBLIC --coverage)
    endif()

    if(COVENT_SENTRY)
        target_include_directories(covent-test SYSTEM PRIVATE sentry-native/include)
        target_compile_definitions(covent-test PRIVATE DWD_GTEST_SENTRY=1)
        target_link_libraries(covent-test PRIVATE sentry::sentry)
    endif()

    include(GoogleTest)
    gtest_discover_tests(covent-test)
endif()