//
// Created by dwd on 11/12/24.
//

#include <covent/http.h>
#include <covent/loop.h>
#include "gtest/gtest.h"

TEST(HTTP, RequestHeaderRead) {
    covent::http::Request req(covent::http::Request::Method::GET, "https://www.google.com");
    std::string_view host = req["Host"];
    EXPECT_EQ(host, "www.google.com");
}

TEST(HTTP, RequestHeaderReadCmp) {
    covent::http::Request req(covent::http::Request::Method::GET, "https://www.google.com");
    EXPECT_EQ(req["Host"], "www.google.com");
    EXPECT_NE(req["Host"], "www.not-google.com");
    EXPECT_LT(req["Host"], "zzzzzzzz");
}

TEST(HTTP, RequestHeaderNonExist) {
    covent::http::Request req(covent::http::Request::Method::GET, "https://www.google.com");
    EXPECT_THROW(std::string_view host = req["Not-Host"], std::runtime_error);
}

TEST(HTTP, RequestHeaderSet) {
    covent::http::Request req(covent::http::Request::Method::GET, "https://www.google.com");
    EXPECT_FALSE(req["New-Header"]);
    req["New-Header"] = "Test Value";
    EXPECT_EQ(req["New-Header"], "Test Value");
}

TEST(HTTP, RequestSimple) {
    covent::Loop loop;
    covent::http::Request req(covent::http::Request::Method::GET, "https://www.google.com");
    auto task = req();
    task.start();
    loop.run_until([&task]() {
        return task.done();
    });
    EXPECT_TRUE(task.done());
    auto resp = task.get();
    EXPECT_EQ(resp.status(), 200);
}
