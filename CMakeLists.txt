cmake_minimum_required(VERSION 3.28)
project(covent)
option(COVENT_BUILD_TESTS "Build the Covent tests" OFF)
option(COVENT_SENTRY "Use Sentry (also for tests)" OFF)
option(COVENT_COVERAGE "Coverage support on library and tests" OFF)
option(BUILD_SHARED_LIBS "Build shared libraries instead of static" OFF)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

find_package(sentry)
find_package(sentry-native)
find_package(Libevent)
find_package(st-sigslot)
find_package(unbound)
find_package(ICU)
find_package(OpenSSL)
find_package(fmt)
find_package(spdlog)
find_package(yaml-cpp)

set(COVENT_HEADERS
        include/covent/covent.h
        include/covent/base.h
        include/covent/coroutine.h
        include/covent/loop.h
        include/covent/core.h
        include/covent/listener.h
        include/covent/sockaddr-cast.h
        include/covent/exceptions.h
        include/covent/http.h
        include/covent/future.h
        include/covent/discovery.h
        include/covent/gather.h
        include/covent/sleep.h
        include/covent/generator.h
        include/covent/crl-cache.h
        include/covent/temp.h)

set(COVENT_SOURCES
        src/covent.cpp
        src/session.cpp
        src/listener.cpp
        src/http/fieldref.cpp
        src/http/request.cpp
        src/http/response.cpp
        src/dns/dns.cc
        src/tls/pkix.cc
        src/tls/crl-cache.cc
        src/http/server.cpp
        src/application.cpp
        src/service.cpp
        src/http/client.cpp
        src/http/message.cpp
        src/http/uri.cpp
        src/sentry-wrap.cc
)

add_library(covent ${COVENT_SOURCES})
add_library(covent::covent ALIAS covent)
target_include_directories(covent PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>  # <prefix>/ include/ mylib
)
target_link_libraries(covent
        PUBLIC libevent::libevent st-sigslot::st-sigslot ICU::uc ICU::i18n OpenSSL::SSL OpenSSL::Crypto fmt::fmt spdlog::spdlog sentry-native::sentry-native
        PRIVATE unbound::unbound)
set_target_properties(covent PROPERTIES EXPORT_NAME covent)

install(DIRECTORY include/covent TYPE INCLUDE)
install(TARGETS covent EXPORT coventTargets LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
install(EXPORT coventTargets FILE covent.cmake NAMESPACE covent:: DESTINATION lib/cmake/covent)
export(EXPORT coventTargets NAMESPACE covent:: FILE "${CMAKE_CURRENT_BINARY_DIR}/covent.cmake")

if(COVENT_COVERAGE)
    target_link_options(covent PUBLIC --coverage)
    target_compile_options(covent PUBLIC --coverage)
endif()

# Tests
if(COVENT_BUILD_TESTS)
    find_package(GTest)
    enable_testing()

    add_executable(covent-test
            test/src/basic.cpp
            test/src/main.cpp
            test/src/coro.cpp
            test/src/echo.cpp
            test/src/http.cpp
            test/src/dns-utils.cc
            test/src/resolver.cc
            test/src/crls.cpp
            test/src/app.cpp
            test/src/service.cpp
            test/src/http-session.cpp
            test/src/uri.cpp
    )
    target_include_directories(covent-test SYSTEM PUBLIC include)
    target_link_libraries(covent-test PUBLIC covent::covent GTest::gtest)
    if(COVENT_COVERAGE)
        target_link_options(covent-test PUBLIC --coverage)
        target_compile_options(covent-test PUBLIC --coverage)
    endif()

    target_include_directories(covent-test SYSTEM PRIVATE sentry-native/include)
    target_compile_definitions(covent-test PRIVATE DWD_GTEST_SENTRY=1)
    target_link_libraries(covent-test PRIVATE sentry::sentry)

    include(GoogleTest)
    gtest_discover_tests(covent-test)
endif()