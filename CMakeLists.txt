cmake_minimum_required(VERSION 3.28)
project(covent)
option(COVENT_BUILD_TESTS "Build the Covent tests" OFF)
option(COVENT_SENTRY "Use Sentry (also for tests)" OFF)
set(COVENT_SIGSLOT_INCLUDE "/usr/include" CACHE STRING "Where to find my sigslot")

set(CMAKE_CXX_STANDARD 26)

if (COVENT_SENTRY)
    set(SENTRY_BACKEND inproc)
    add_subdirectory(sentry-native)
endif()

find_package(PkgConfig)

pkg_check_modules(EVENT libevent libevent_openssl libevent_pthreads REQUIRED)

add_library(covent SHARED src/covent.cpp include/covent/covent.h
        include/covent/base.h
        include/covent/coroutine.h
        include/covent/loop.h
        include/covent/core.h
        include/covent/listener.h
        src/session.cpp
        src/listener.cpp)
target_include_directories(covent PUBLIC include)
target_include_directories(covent PUBLIC ${EVENT_INCLUDE_DIRS} ${COVENT_SIGSLOT_INCLUDE})
target_link_libraries(covent PUBLIC ${EVENT_LDFLAGS})

# Tests
if(COVENT_BUILD_TESTS)
    include(FetchContent)
    FetchContent_Declare(
            googletest
            URL https://github.com/google/googletest/archive/main.zip
            DOWNLOAD_EXTRACT_TIMESTAMP ON
            EXCLUDE_FROM_ALL
    )
    FetchContent_MakeAvailable(googletest)

    enable_testing()

    add_executable(covent-test
            test/basic.cpp
            test/main.cpp
            test/coro.cpp
            test/echo.cpp
    )
    target_include_directories(covent-test SYSTEM PUBLIC include)
    target_link_libraries(covent-test PUBLIC covent GTest::gtest)

    if(COVENT_SENTRY)
        target_include_directories(covent-test SYSTEM PRIVATE sentry-native/include)
        target_compile_definitions(covent-test PRIVATE DWD_GTEST_SENTRY=1)
        target_link_libraries(covent-test PRIVATE sentry::sentry)
    endif()

    include(GoogleTest)
    gtest_discover_tests(covent-test)
endif()