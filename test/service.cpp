//
// Created by dwd on 12/30/24.
//

#include <gtest/gtest.h>

#include "covent/service.h"

// These tests have an extra S because otherwise CLion gets confused.

GTEST_TEST(SService, resolver_default) {
    covent::Loop loop;
    covent::Service serv;
    serv.add(std::make_unique<covent::dns::Resolver>(false));
    auto & res = serv.resolver("dave.cridland.net");
    auto rr = loop.run_task(res.address_v4("dave.cridland.net"));
}

GTEST_TEST(SService, resolver_wildcard) {
    covent::Loop loop;
    covent::Service serv;
    serv.add(std::make_unique<covent::dns::Resolver>(false), "*.cridland.net");
    auto & res = serv.resolver("dave.cridland.net");
    auto rr = loop.run_task(res.address_v4("dave.cridland.net"));
}

GTEST_TEST(SService, resolver_exact) {
    covent::Loop loop;
    covent::Service serv;
    serv.add(std::make_unique<covent::dns::Resolver>(false), "dave.cridland.net");
    auto & res = serv.resolver("dave.cridland.net");
    auto rr = loop.run_task(res.address_v4("dave.cridland.net"));
}
// Now a default from the loop.
// GTEST_TEST(SService, resolver_mismatch) {
//     covent::Loop loop;
//     covent::Service serv;
//     serv.add(std::make_unique<covent::dns::Resolver>(false), "cridland.net");
//     EXPECT_ANY_THROW(
//     auto & res = serv.resolver("dave.cridland.net");
//     );
// }

GTEST_TEST(SService, gather) {
    covent::Loop loop;
    covent::Service serv;
    serv.add(std::make_unique<covent::dns::Resolver>(false));
    auto foo = loop.run_task(serv.discovery("dave.cridland.net"));
    EXPECT_EQ(foo.gathered_tlsa.size(), 0);
    EXPECT_EQ(foo.gathered_connect.size(), 4); // StartTLS and DirectTLS for both v4 and v6.
}
