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
    EXPECT_EQ(address, "88.98.37.177");
}

TEST(Resolver, wiggle_v6) {
    covent::Loop loop;
    auto task = wiggle_v6();
    task.start();
    while(!task.done()) {
        loop.run_once(true);
    }
    auto result = task.get();
    EXPECT_EQ(result.domain, "");
    EXPECT_EQ(result.dnssec, false); // No DNSSEC keys, so no validation
    EXPECT_EQ(result.error, "No records present");
    ASSERT_EQ(result.addr.size(), 0);
    // auto address = covent::address_tostring(&result.addr[0]);
    // EXPECT_EQ(address, "2a02:8010:800b::1");
}

TEST(Resolver, faked_data) {
    covent::Loop loop;
    covent::dns::Resolver res(false);
    res.add_data("test.example. IN A 127.127.127.127");
    auto result = loop.run_task(res.address_v4("test.example"));
    EXPECT_TRUE(result.error.empty());
    EXPECT_EQ(result.dnssec, false); // Unfortunately.
    EXPECT_EQ(result.addr.size(), 1);
    EXPECT_EQ(covent::address_tostring(&result.addr[0]), "127.127.127.127");
}

TEST(Resolver, inject) {
    covent::Loop loop;
    covent::dns::Resolver res{false};
    covent::dns::rr::SRV srv_rr = {
        "test.foo.example",
        12,
        13,
        14,
        "foo"
    };
    covent::dns::answers::SRV srv = {
        {
            srv_rr
        },
        "foo.example",
        true,
        ""
    };
    res.inject(srv);
    {
        auto result = loop.run_task(res.srv("foo", "foo.example"));
        ASSERT_NE(result.rrs.size(), 0);
        EXPECT_EQ(result.rrs[0].port, 12);
    }
    {
        auto result = loop.run_task(res.srv("not-foo", "foo.example"));
        EXPECT_NE(result.error, "");
    }
    {
        auto result = loop.run_task(res.srv("foo", "dave.cridland.net"));
        ASSERT_NE(result.rrs.size(), 0);
        EXPECT_EQ(result.rrs[0].port, 12);
    }
}