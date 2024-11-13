//
// Created by dwd on 11/12/24.
//
#include <event2/http.h>

#include "covent/http.h"
#include "covent/covent.h"

using namespace covent::http;

namespace {
    void request_complete(struct evhttp_request *, void * v) {
        auto req = reinterpret_cast<Request *>(v);
        req->complete();
    }
}
Request::Request(covent::http::Request::Method m, std::string uri) : method(m), m_uri(std::move(uri)) {
    m_request = evhttp_request_new(request_complete, this);
    auto parsed = evhttp_uri_parse(m_uri.c_str());
    evhttp_add_header(evhttp_request_get_output_headers(m_request), "Host", evhttp_uri_get_host(parsed));
}

void Request::complete() {
}

FieldRef Request::operator[](const std::string & name) {
    auto header = evhttp_request_get_output_headers(m_request);
    return FieldRef(header, name);
}

ConstFieldRef Request::operator[](const std::string & name) const {
    auto header = evhttp_request_get_output_headers(m_request);
    return ConstFieldRef(header, name);
}

covent::task<Response> Request::operator()() const {
    auto & loop = Loop::thread_loop();
    auto parsed = evhttp_uri_parse(m_uri.c_str());
    auto port = evhttp_uri_get_port(parsed);
    port != -1 ? port : 80;
    auto conn = evhttp_connection_base_new(loop.event_base(), nullptr, evhttp_uri_get_host(parsed), port);
    evhttp_make_request(conn, m_request, EVHTTP_REQ_GET, evhttp_uri_get_path(parsed));
    co_await m_completed;
}