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

#include <covent/gather.h>
#include <covent/generator.h>
#include <random>
#include <ranges>
#include <algorithm>

namespace {
    covent::generator_async<covent::dns::rr::SRV> srv_lookup(std::string const & domain, std::vector<std::string> const & services) {
        auto & resolver = covent::Loop::thread_loop().default_resolver();
        auto srv_list = co_await covent::gather(
                services | std::ranges::views::transform([&domain, &resolver](auto const & arg) {
                    return resolver.srv(arg, domain);
                })
        );
        auto it = srv_list.begin();
        auto srv_def = *it++;
        auto srv_tls = *it;
        for (auto & rr : srv_tls.rrs) {
            rr.tls = true;
            srv_def.rrs.push_back(rr);
        }
        std::sort(srv_def.rrs.begin(), srv_def.rrs.end(), [](auto const & lhs, auto const & rhs) {
           return lhs.priority < rhs.priority;
        });
        std::random_device rd;
        std::mt19937 gen(rd());
        while (srv_def.rrs.size() > 0) {
            auto prio = srv_def.rrs[0].priority;
            int weight_total = 0;
            for (auto const & rr : srv_def.rrs) {
                if (prio != rr.priority) break;
                weight_total += rr.weight;
            }
            if (weight_total == 0) {
                auto rr = srv_def.rrs[0];
                srv_def.rrs.erase(srv_def.rrs.begin());
                co_yield rr;
            }
            std::uniform_int_distribution<> distrib(0, weight_total);
            auto weight_sel = distrib(gen);
            for (auto i = srv_def.rrs.begin(); i != srv_def.rrs.end(); ++i) {
                weight_sel -= i->weight;
                if (weight_sel <= 0) {
                    auto rr = *i;
                    srv_def.rrs.erase(i);
                    co_yield rr;
                }
            }
        }
    }
}
