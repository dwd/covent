//
// Created by dwd on 11/12/24.
//
#include <event2/http.h>
#include <event2/buffer.h>

#include <covent/http.h>

using namespace covent::http;

Response::Response(std::unique_ptr<Message> && m) : m_response(std::move(m)) {}

int Response::status() const {
    return m_response->status_code;
}

FieldRef Response::operator[](const std::string & name) {
    return FieldRef(m_response->header, name);
}

ConstFieldRef Response::operator[](const std::string & name) const {
    return ConstFieldRef(m_response->header, name);
}

std::string_view Response::body() const {
    return m_response->body;
}
