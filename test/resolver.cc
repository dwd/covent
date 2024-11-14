#include <covent/loop.h>
#include "covent/dns.h"

#include "gtest/gtest.h"

namespace {
    covent::task<covent::dns::answers::Address> wiggle_v4() {
        covent::dns::Resolver r(true, false, "../test/dnskeys");
        co_return co_await r.address_v4("wiggle.cridland.io");
    }
    covent::task<covent::dns::answers::Address> wiggle_v6() {
        covent::dns::Resolver r(false, false, {});
        co_return co_await r.address_v6("wiggle.cridland.io");
    }
}

TEST(Resolver, wiggle_v4) {
    covent::Loop loop;
    auto task = wiggle_v4();
    task.start();
    while(!task.done()) {
        loop.run_once(true);
    }
    auto result = task.get();
    EXPECT_EQ(result.domain, "wiggle.cridland.io");
    EXPECT_EQ(result.dnssec, true);
    EXPECT_EQ(result.error, "");
    ASSERT_EQ(result.addr.size(), 1);
    auto address = covent::address_tostring(&result.addr[0]);
    EXPECT_EQ(address, "217.155.137.62");
}

TEST(Resolver, wiggle_v6) {
    covent::Loop loop;
    auto task = wiggle_v6();
    task.start();
    while(!task.done()) {
        loop.run_once(true);
    }
    auto result = task.get();
    EXPECT_EQ(result.domain, "wiggle.cridland.io");
    EXPECT_EQ(result.dnssec, false); // No DNSSEC keys, so no validation
    EXPECT_EQ(result.error, "");
    ASSERT_EQ(result.addr.size(), 1);
    auto address = covent::address_tostring(&result.addr[0]);
    EXPECT_EQ(address, "2a02:8010:800b::1");
}
