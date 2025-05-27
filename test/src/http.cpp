//
// Created by dwd on 11/12/24.
//

#include <covent/http.h>
#include <covent/loop.h>
#include "gtest/gtest.h"

TEST(HTTP, RequestHeaderRead) {
    covent::Loop loop;
    covent::http::Request req(covent::http::Method::GET, "https://www.google.com");
    std::string_view host = req["Host"];
    EXPECT_EQ(host, "www.google.com");
}

TEST(HTTP, RequestHeaderReadCmp) {
    covent::Loop loop;
    covent::http::Request req(covent::http::Method::GET, "https://www.google.com");
    EXPECT_EQ(req["Host"], "www.google.com");
    EXPECT_NE(req["Host"], "www.not-google.com");
    EXPECT_LT(req["Host"], "zzzzzzzz");
}

TEST(HTTP, RequestHeaderNonExist) {
    covent::Loop loop;
    covent::http::Request req(covent::http::Method::GET, "https://www.google.com");
    EXPECT_THROW(std::string_view host = req["Not-Host"], std::runtime_error);
}

TEST(HTTP, RequestHeaderSet) {
    covent::Loop loop;
    covent::http::Request req(covent::http::Method::GET, "https://www.google.com");
    EXPECT_FALSE(req["New-Header"]);
    req["New-Header"] = "Test Value";
    EXPECT_EQ(req["New-Header"], "Test Value");
}

TEST(HTTP, RequestSimple) {
    covent::Loop loop;
    covent::http::Request req(covent::http::Method::GET, "https://www.google.com");
    auto resp = loop.run_task(req());
    EXPECT_EQ(resp->status(), 200);
}

TEST(HTTP_Client, RequestSimple) {
    covent::Loop loop;
    auto & service = loop.http_service();
    auto & entry = service.entry("*.google.com");
    entry.make_resolver(false);
    entry.make_tls_context(true, true, "localhost");
    entry.make_validator(true, true);
    covent::http::Client client(service, "https://www.google.com");
    auto req = client.request(covent::http::Method::GET, "https://www.google.com", {});
    auto resp = loop.run_task(req());
    EXPECT_EQ(resp->status(), 200);
    auto req2 = client.request(covent::http::Method::GET, "https://www.google.com", {});
    auto resp2 = loop.run_task(req2());
    EXPECT_EQ(resp->status(), 200);
}

TEST(HTTP_Server, MissingRoot) {
    covent::Loop loop;
    covent::http::Server srv(8001, false);
    loop.run_once(false);
    covent::http::Request req(covent::http::Method::GET, "http://localhost:8001/");
    auto resp = loop.run_task(req());
    EXPECT_EQ(resp->status(), 500);
}

TEST(HTTP_Server, NotFound) {
    covent::Loop loop;
    covent::http::Server srv(8001, false);
    srv.add(std::make_unique<covent::http::Endpoint>("/"));
    covent::http::Request req(covent::http::Method::GET, "http://localhost:8001/");
    auto resp = loop.run_task(req());
    EXPECT_EQ(resp->status(), 404);
}

TEST(HTTP_Server, Path) {
    covent::Loop loop;
    covent::http::Server srv(8001, false);
    srv.add(std::make_unique<covent::http::Endpoint>("/"));
    srv.add(std::make_unique<covent::http::Endpoint>("/test", [](evhttp_request * req) -> covent::task<int> {
        evhttp_send_reply(req, 201, "OK", nullptr);
        co_return 201;
    }));
    {
        covent::http::Request req(covent::http::Method::GET, "http://localhost:8001/");
        auto resp = loop.run_task(req());
        EXPECT_EQ(resp->status(), 404);
    }
    {
        covent::http::Request req(covent::http::Method::GET, "http://localhost:8001/not-found");
        auto resp = loop.run_task(req());
        EXPECT_EQ(resp->status(), 404);
    }
    {
        covent::http::Request req(covent::http::Method::GET, "http://localhost:8001/test");
        auto resp = loop.run_task(req());
        EXPECT_EQ(resp->status(), 201);
    }
}

TEST(HTTP_Server, ExplicitClient) {
    covent::Loop loop;
    covent::http::Server srv(8001, false);
    srv.add(std::make_unique<covent::http::Endpoint>("/"));
    srv.add(std::make_unique<covent::http::Endpoint>("/test", [](evhttp_request * req) -> covent::task<int> {
        evhttp_send_reply(req, 201, "OK", nullptr);
        co_return 201;
    }));
    covent::Service service;
    covent::http::Client client{service, "http://localhost:8001/"};
    {
        auto req = client.request(covent::http::Method::GET, "http://localhost:8001/");
        auto resp = loop.run_task(req());
        EXPECT_EQ(resp->status(), 404);
    }
    {
        auto req = client.request(covent::http::Method::GET, "http://localhost:8001/not-found");
        auto resp = loop.run_task(req());
        EXPECT_EQ(resp->status(), 404);
    }
    {
        auto req = client.request(covent::http::Method::GET, "http://localhost:8001/test");
        auto resp = loop.run_task(req());
        EXPECT_EQ(resp->status(), 201);
    }
}

TEST(HTTP_Server, Middleware) {
    covent::Loop loop;
    covent::http::Server srv(8001, false);
    srv.add(std::make_unique<covent::http::Endpoint>("/"));
    srv.add(std::make_unique<covent::http::Endpoint>("/test", [](evhttp_request * req) -> covent::task<int> {
        evhttp_send_reply(req, 201, "OK", nullptr);
        co_return 201;
    }));
    auto new_endpoint = std::make_unique<covent::http::Endpoint>("/test2", [](evhttp_request * req) -> covent::task<int> {
        evhttp_send_reply(req, 200, "OK", nullptr);
        co_return 200;
    });
    new_endpoint->add(std::make_unique<covent::http::Middleware>([](evhttp_request * req) -> covent::task<void> {
        throw covent::http::exception::http_status(401, "Not allowed");
    }));
    srv.add(std::move(new_endpoint));
    {
        covent::http::Request req(covent::http::Method::GET, "http://localhost:8001/");
        auto resp = loop.run_task(req());
        EXPECT_EQ(resp->status(), 404);
    }
    {
        covent::http::Request req(covent::http::Method::GET, "http://localhost:8001/not-found");
        auto resp = loop.run_task(req());
        EXPECT_EQ(resp->status(), 404);
    }
    {
        covent::http::Request req(covent::http::Method::GET, "http://localhost:8001/test");
        auto resp = loop.run_task(req());
        EXPECT_EQ(resp->status(), 201);
    }
    {
        covent::http::Request req(covent::http::Method::GET, "http://localhost:8001/test2");
        auto resp = loop.run_task(req());
        EXPECT_EQ(resp->status(), 401);
    }
}
