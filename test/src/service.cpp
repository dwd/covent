//
// Created by dwd on 12/30/24.
//

#include <gtest/gtest.h>

#include "covent/service.h"

// These tests have an extra S because otherwise CLion gets confused.

GTEST_TEST(SService, resolver_default) {
    covent::Loop loop;
    covent::Service serv;
    serv.add("");
    auto & res = serv.entry("dave.cridland.net");
    EXPECT_EQ(res.name(), "");
}

GTEST_TEST(SService, resolver_wildcard) {
    covent::Loop loop;
    covent::Service serv;
    serv.add("*.cridland.net");
    auto & res = serv.entry("dave.cridland.net");
    EXPECT_EQ(res.name(), "*.cridland.net");
}

GTEST_TEST(SService, resolver_exact) {
    covent::Loop loop;
    covent::Service serv;
    serv.add("dave.cridland.net");
    auto & res = serv.entry("dave.cridland.net");
    EXPECT_EQ(res.name(), "dave.cridland.net");
}

GTEST_TEST(SService, resolver_wild_plus) {
    covent::Loop loop;
    covent::Service serv;
    serv.add("");
    serv.add("*.dave..cridland.net");
    auto & res = serv.entry("dave.cridland.net");
    EXPECT_EQ(res.name(), "");
}
// Now a default from the loop.
GTEST_TEST(SService, resolver_mismatch) {
    covent::Loop loop;
    covent::Service serv;
    serv.add("cridland.net");
    auto & res = serv.entry("dave.cridland.net");
    EXPECT_EQ(res.name(), "");
}

GTEST_TEST(SService, gather) {
    covent::Loop loop;
    covent::Service serv;
    serv.add("");
    serv.add("dave.cridland.net");
    // PKIX discovery, so no names/TLSA expected from DNS due to no DNSSEC.
    {
        auto [gathered_hosts, gathered_tlsa] = loop.run_task(serv.entry("dave.cridland.net").discovery("dave.cridland.net"));
        EXPECT_EQ(gathered_tlsa.size(), 0);
        EXPECT_EQ(gathered_hosts.size(), 0);
    }
    covent::dns::answers::SRV faked;
    faked.dnssec = true;
    faked.rrs.emplace_back(
        "dave.cridland.net",
        111,
        1,
        1,
        "xmpp-server"
    );
    faked.rrs.emplace_back(
        "dave.cridland.net",
        222,
        1,
        0,
        "xmpps-server"
    );
    serv.entry("dave,cridland.net").resolver().inject(faked);
    {
        auto [gathered_hosts, gathered_tlsa] = loop.run_task(serv.entry("dave.cridland.net").discovery("dave.cridland.net"));
        EXPECT_EQ(gathered_tlsa.size(), 0);
        EXPECT_EQ(gathered_hosts.size(), 1);
    }
    auto const & fn = [&serv]() -> covent::task<void> {
        auto srv_gen = serv.entry("dave.cridland.net").srv_lookup("dave.cridland.net", {"xmpp-server", "xmpps-server"});
        bool start = true;
        for (auto it = co_await srv_gen.begin(); it != srv_gen.end(); co_await ++it) {
            if (start) {
                EXPECT_EQ((*it).port, 222);
                start = false;
            } else {
                EXPECT_EQ((*it).port, 111);
            }
        }
    };
    loop.run_task(fn());
    // auto const & fn2 = [&serv]() -> covent::task<void> {
    //     auto srv_gen = serv.entry("dave.cridland.net").xmpp_lookup("dave.cridland.net");
    //     bool start = true;
    //     for (auto it = co_await srv_gen.begin(); it != srv_gen.end(); co_await ++it) {
    //         if (start) {
    //             EXPECT_EQ((*it).port, 222);
    //             start = false;
    //         } else {
    //             EXPECT_EQ((*it).port, 222);
    //         }
    //     }
    // };
    // loop.run_task(fn2());
}
