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
find_package(ICU)
find_package(OpenSSL)

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
        src/dns/discovery.cpp
        src/tls/pkix.cc
        src/tls/crl-cache.cc
        include/covent/sleep.h
        src/http/server.cpp)

#add_library(covent SHARED ${COVENT_SOURCES})
add_library(covent_static STATIC ${COVENT_SOURCES})
#add_library(covent::covent ALIAS covent)
add_library(covent::covent_static ALIAS covent_static)
#target_include_directories(covent PUBLIC
#        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
#        $<INSTALL_INTERFACE:include>  # <prefix>/ include/ mylib
#)
target_include_directories(covent_static PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>  # <prefix>/ include/ mylib
)
#target_link_libraries(covent PUBLIC libevent::libevent sigslot::sigslot unbound::unbound ICU::uc ICU::i18n OpenSSL::SSL OpenSSL::Crypto)
target_link_libraries(covent_static PUBLIC libevent::libevent sigslot::sigslot unbound::unbound ICU::uc ICU::i18n OpenSSL::SSL OpenSSL::Crypto)
#set_target_properties(covent PROPERTIES EXPORT_NAME covent)

install(DIRECTORY include/covent TYPE INCLUDE)
#install(TARGETS covent EXPORT coventTargets LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
install(TARGETS covent_static EXPORT coventTargets LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
install(EXPORT coventTargets FILE covent.cmake NAMESPACE covent:: DESTINATION lib/cmake/covent)
export(EXPORT coventTargets NAMESPACE covent:: FILE "${CMAKE_CURRENT_BINARY_DIR}/covent.cmake")

if(COVENT_COVERAGE)
    target_link_options(covent_static PUBLIC --coverage)
    target_link_options(covent PUBLIC --coverage)
    target_compile_options(covent_static PUBLIC --coverage)
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
            test/crls.cpp
    )
    target_include_directories(covent-test SYSTEM PUBLIC include)
    target_link_libraries(covent-test PUBLIC covent_static GTest::gtest)
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