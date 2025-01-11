//
// Created by dwd on 12/31/24.
//


#include <iostream>
#include <covent/http.h>
#include <covent/loop.h>
#include <covent/core.h>
#include <event2/bufferevent_ssl.h>
#include <fmt/format.h>

#include "covent/sleep.h"

using namespace covent::http;

namespace {
    class HTTPSession : public covent::Session {
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

}

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

covent::task<std::unique_ptr<Response>> Client::send(Request &r) {
    bool ssl = false;
    if (r.uri().scheme == "https") {
        ssl = true;
    }

    {
        auto session = m_loop.add<HTTPSession>(m_loop);
        auto address = co_await m_resolver.address_v6(r.uri().host);
        for (auto & s : address.addr) {
            try {
                sockaddr_cast<AF_INET6>(&s)->sin6_port = htons(r.uri().port.value());
                co_await session->connect(&s);
                std::cerr << "Connected" << std::endl;
                if (ssl) {
                    std::cerr << "SSL start" << std::endl;
                    co_await session->ssl(m_tls_context.instantiate(true, r.uri().host), true);
                    std::cerr << "SSL end" << std::endl;
                    co_await m_validator.verify_tls(session->ssl(), r.uri().host);
                    std::cerr << "SSL validation" << std::endl;
                }
                co_await covent::sleep(0L);
                std::cerr << "Session ready" << std::endl;
                break;
            } catch (std::runtime_error &) {
                continue;
            }
        }
        session->message = std::make_unique<Message>();
        r.send(*session);
        co_await session->ready;
        co_return std::make_unique<Response>(std::move(session->message));
    }
}
