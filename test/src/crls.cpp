//
// Created by dwd on 11/27/24.
//

// http://x1.c.lencr.org/

#include <gtest/gtest.h>
#include <covent/loop.h>
#include <covent/crl-cache.h>

TEST(CrlCache, simple) {
    covent::Loop loop;
    std::string const uri = "http://x1.c.lencr.org/";
    auto [one, two, three] = loop.run_task(covent::pkix::CrlCache::crl(uri));
    EXPECT_EQ(one, uri);
    EXPECT_EQ(two, 200);
    EXPECT_NE(three, nullptr);
}
