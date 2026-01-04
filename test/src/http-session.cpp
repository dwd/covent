//
// Created by dwd on 1/2/25.
//


#include <covent/core.h>
#include <covent/loop.h>
#include <covent/http.h>
#include <fmt/format.h>
#include <gtest/gtest.h>

GTEST_TEST(http_session, simple) {
    covent::http::Message message;
    EXPECT_EQ(0, message.process("HTTP/1.1 200 OK")); // No CRLF
    EXPECT_EQ(17, message.process("HTTP/1.1 200 OK\r\nContent-Type")); // Just status line consumed
    EXPECT_EQ(message.status_code, 200);
    EXPECT_EQ(message.status_text, "OK");
    EXPECT_EQ(message.header.size(), 0);
    EXPECT_EQ(26, message.process("Content-Type: text/plain\r\n"));
    EXPECT_EQ(message.status_code, 200);
    EXPECT_EQ(message.status_text, "OK");
    EXPECT_EQ(message.header.size(), 1);
    EXPECT_EQ(message.header.contains("content-type"), true);
    EXPECT_EQ(message.header["content-type"], "text/plain");
    EXPECT_EQ(24, message.process("Content-Length: 5\r\n\r\nabc"));
    EXPECT_EQ(message.status_code, 200);
    EXPECT_EQ(message.status_text, "OK");
    // EXPECT_EQ(sess->message.header_read, true);
    EXPECT_EQ(message.header.size(), 2);
    EXPECT_EQ(message.header.contains("content-type"), true);
    EXPECT_EQ(message.header.contains("content-length"), true);
    EXPECT_EQ(message.header["content-length"], "5");
    // EXPECT_EQ(sess->message.body_remaining.has_value(), true);
    // EXPECT_EQ(sess->message.body_remaining.value(), 2);
    EXPECT_EQ(message.body, "abc");
}

GTEST_TEST(http_session, chunked) {
    covent::http::Message message;
    std::string data_in{"HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nabc;e\r\n0\r\n\r\n"};
    EXPECT_EQ(data_in.length(), message.process(data_in));
    EXPECT_EQ(message.body, "abc;e");
    EXPECT_EQ(message.complete, true);
}

GTEST_TEST(http_session, chunked_split) {
    covent::http::Message message;
    std::string data_in{"HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nabc;e\r\n0\r\n\r\n"};
    std::string active;
    unsigned long total = 0;
    for (auto const c : data_in) {
        active += c;
        auto used = message.process(active);
        active = active.substr(used);
        total += used;
    }
    EXPECT_EQ(data_in.length(), total);
    EXPECT_EQ(message.body, "abc;e");
    EXPECT_EQ(message.complete, true);
}

GTEST_TEST(http_session, render) {
    covent::http::Message message("http://www.google.com");
    EXPECT_EQ(message.render_request(covent::http::Method::GET), "GET / HTTP/1.1\r\n");
    EXPECT_EQ(message.render_header(), "host: www.google.com\r\n\r\n");
    EXPECT_EQ(message.render_body(), "");
}

// GTEST_TEST(http_session, live) {
//     covent::Loop loop;
//     auto sess = loop.add<HTTPSession>(loop);
//     // Do some resolution;
//     covent::dns::Resolver resolver(false);
//     auto result = loop.run_task(resolver.address_v6("www.google.com"));
//     covent::sockaddr_cast<AF_INET6>(&result.addr[0])->sin6_port = htons(80);
//     // Connect
//     loop.run_task(sess->connect(&result.addr[0]));
//     // Send the request and wait
//     auto response = loop.run_task(sess->send_request("http://www.google.com/"));
//     EXPECT_EQ(response.status_code, 200);
// }
