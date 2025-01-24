//
// Created by dwd on 12/31/24.
//


#include <iostream>
#include <covent/http.h>
#include <covent/loop.h>
#include <covent/core.h>
#include <covent/gather.h>
#include <event2/bufferevent_ssl.h>
#include <fmt/format.h>

#include "covent/sleep.h"

using namespace covent::http;

class Client::HTTPSession : public covent::Session {
public:
    using covent::Session::Session;

    std::unique_ptr<covent::http::Message> message;
    covent::future<bool> ready;

    covent::task<unsigned long> process(std::string_view data) override {
        auto tmp = message->process(data);
        if (message->complete) {
            ready.resolve(true);
        }
        co_return tmp;
    }
};

Client::Client(covent::dns::Resolver & r, covent::pkix::PKIXValidator & v, covent::pkix::TLSContext & t, URI const & uri)
    : m_loop(covent::Loop::thread_loop()), m_uri(uri), m_resolver(r), m_validator(v), m_tls_context(t) {
}

Client::Client(covent::dns::Resolver & r, covent::pkix::PKIXValidator & v, covent::pkix::TLSContext & t, std::string_view uri)
    : m_loop(covent::Loop::thread_loop()), m_uri(uri), m_resolver(r), m_validator(v), m_tls_context(t) {
}

Request Client::request(Method method, URI const & uri, std::optional<std::string> body) {
    return Request(*this, method, uri);
}

Request Client::request(Method method, std::string_view uri, std::optional<std::string> body) {
    return Request(*this, method, uri);
}

covent::task<std::shared_ptr<Client::HTTPSession>> Client::connect(covent::dns::answers::Address address, URI const & uri) const {
    bool ssl = false;
    if (uri.scheme == "https") {
        ssl = true;
    }
    auto session = std::make_shared<HTTPSession>(m_loop); // Don't add it to the loop yet!
    for (auto & s : address.addr) {
        try {
            covent::sockaddr_cast<AF_INET6>(&s)->sin6_port = htons(uri.port.value());
            co_await session->connect(&s);
            std::cerr << "Connected" << std::endl;
            if (ssl) {
                std::cerr << "SSL start" << std::endl;
                co_await session->ssl(m_tls_context.instantiate(true, uri.host), true);
                std::cerr << "SSL end" << std::endl;
                co_await m_validator.verify_tls(session->ssl(), uri.host);
                std::cerr << "SSL validation" << std::endl;
            }
            std::cerr << "Session ready" << std::endl;
            break;
        } catch (std::runtime_error e) {}
    }
    m_loop.add(session);
    co_return session;
}

covent::task<std::shared_ptr<Client::HTTPSession>> Client::connect_v4(URI const & uri) const {
    co_return co_await connect(co_await m_resolver.address_v4(uri.host), uri);
}

covent::task<std::shared_ptr<Client::HTTPSession>> Client::connect_v6(URI const & uri) const {
    co_return co_await connect(co_await m_resolver.address_v6(uri.host), uri);
}

covent::task<std::unique_ptr<Response>> Client::send(Request &r) {
    std::shared_ptr<Client::HTTPSession> session;
    std::list<task<std::shared_ptr<HTTPSession>>> attempts; attempts.emplace_back(connect_v6(r.uri()));

    try {
        session = co_await race<std::shared_ptr<HTTPSession>>(attempts, 0.1);
    } catch (covent::race_timeout &) {
        // Carry on!
    }
    if (!session) {
        attempts.emplace_back(connect_v4(r.uri()));
        attempts.rbegin()->start();
        session = co_await race<std::shared_ptr<HTTPSession>>(attempts, 5, false);
    }
    session->message = std::make_unique<Message>();
    r.send(*session);
    co_await session->ready;
    co_return std::make_unique<Response>(std::move(session->message));
}
