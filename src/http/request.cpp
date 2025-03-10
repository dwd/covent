//
// Created by dwd on 11/12/24.
//
#include <event2/http.h>
#include <event2/bufferevent.h>
#include <event2/bufferevent_ssl.h>

#include "covent/http.h"
#include "covent/covent.h"

using namespace covent::http;

Request::Request(Method m, URI const & uri)
: method(m), m_loop(Loop::thread_loop()), m_request(std::make_unique<Message>(uri)), m_client_temp(
      std::make_unique<Client>(m_loop.http_service(), uri)), m_client(*m_client_temp)  {
}

Request::Request(Method m, std::string_view uri)
: method(m), m_loop(Loop::thread_loop()), m_request(std::make_unique<Message>(uri)), m_client_temp(
      std::make_unique<Client>(m_loop.http_service(), uri)), m_client(*m_client_temp)  {
}

Request::Request(Client & client, Method m, URI const & uri)
: method(m), m_loop(Loop::thread_loop()), m_request(std::make_unique<Message>(uri)), m_client(client) {
}

void Request::complete() {
    m_completed.resolve();
}

FieldRef Request::operator[](const std::string & name) {
    return FieldRef(m_request->header, name);
}

ConstFieldRef Request::operator[](const std::string & name) const {
    return ConstFieldRef(m_request->header, name);
}

covent::task<std::unique_ptr<Response>> Request::operator()() {
    co_return co_await m_client.send(*this);
}

void Request::send(Session & sess) const {
    sess.write(m_request->render_request());
    sess.write(m_request->render_header());
    sess.write(m_request->render_body());
}