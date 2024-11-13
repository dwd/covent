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
    m_completed.resolve();
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
    port = port != -1 ? port : 80;
    auto conn = evhttp_connection_base_new(loop.event_base(), nullptr, evhttp_uri_get_host(parsed), port);
    std::string path{"/"};
    const char * path_str = path.c_str();
    if (auto p = evhttp_uri_get_path(parsed); p && p[0]) {
        path_str = p;
    }
    if (auto p = evhttp_uri_get_query(parsed); p && p[0]) {
        if (path_str != path.c_str()) {
            path = path_str;
        }
        path += '?';
        path += p;
        path_str = path.c_str();
    }
    evhttp_make_request(conn, m_request, EVHTTP_REQ_GET, path_str);
    co_await m_completed;
    co_return Response(m_request);
}