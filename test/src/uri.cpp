#include <covent/http.h>
#include <gtest/gtest.h>
//
// Created by dwd on 1/10/25.
//


GTEST_TEST(http_session, uri_simple) {
    covent::http::URI uri{"http://www.google.com/"};
    EXPECT_EQ(uri.host, "www.google.com");
    EXPECT_EQ(uri.netloc, "www.google.com");
    EXPECT_EQ(uri.port, 80);
    EXPECT_EQ(uri.path, "/");
}
GTEST_TEST(http_session, uri_https_no_path) {
    covent::http::URI uri{"https://www.google.com"};
    EXPECT_EQ(uri.host, "www.google.com");
    EXPECT_EQ(uri.netloc, "www.google.com");
    EXPECT_EQ(uri.port, 443);
    EXPECT_EQ(uri.path, "/");
}
GTEST_TEST(http_session, uri_https_explicit_port) {
    covent::http::URI uri{"https://www.google.com:8443/path"};
    EXPECT_EQ(uri.host, "www.google.com");
    EXPECT_EQ(uri.netloc, "www.google.com:8443");
    EXPECT_EQ(uri.port, 8443);
    EXPECT_EQ(uri.path, "/path");
}
GTEST_TEST(http_session, uri_https_explicit_port_no_path) {
    covent::http::URI uri{"https://www.google.com:8443"};
    EXPECT_EQ(uri.host, "www.google.com");
    EXPECT_EQ(uri.netloc, "www.google.com:8443");
    EXPECT_EQ(uri.port, 8443);
    EXPECT_EQ(uri.path, "/");
}
GTEST_TEST(http_session, uri_missing_slashes) {
    covent::http::URI uri{"https:/www.google.com"};
    EXPECT_EQ(uri.host, "www.google.com");
    EXPECT_EQ(uri.netloc, "www.google.com");
    EXPECT_EQ(uri.port, 443);
    EXPECT_EQ(uri.path, "/");
}
GTEST_TEST(http_session, uri_missing_scheme) {
    EXPECT_ANY_THROW(
    covent::http::URI uri{"www.google.com"};
    );
}
GTEST_TEST(http_session, uri_just_path) {
    EXPECT_ANY_THROW(
    covent::http::URI uri{"/www.google.com"};
    );
}
