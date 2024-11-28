//
// Created by dwd on 11/12/24.
//
#include <event2/http.h>
#include <event2/buffer.h>

#include <covent/http.h>

using namespace covent::http;

Response::Response(evhttp_request * r) : m_request(r), m_body(body_low()) {
    evhttp_request_own(m_request);
}

Response::~Response() {
    // This results in a double-free, despite my best efforts.
    // evhttp_request_free(m_request);
}

int Response::status() const {
    return evhttp_request_get_response_code(m_request);
}

FieldRef Response::operator[](const std::string & name) {
    auto header = evhttp_request_get_input_headers(m_request);
    return FieldRef(header, name);
}

ConstFieldRef Response::operator[](const std::string & name) const {
    auto header = evhttp_request_get_input_headers(m_request);
    return ConstFieldRef(header, name);
}

std::string_view Response::body_low() const {
    auto buf = evhttp_request_get_input_buffer(m_request);
    return evbuffer_get_length(buf) > 0 ? std::string_view{reinterpret_cast<char *>(evbuffer_pullup(buf, -1)),
                                                           evbuffer_get_length(buf)} : std::string_view{};
}