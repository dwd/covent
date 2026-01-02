//
// Created by dwd on 12/31/24.
//


#include <covent/app.h>
#include <covent/http.h>
#include <covent/loop.h>
#include <covent/core.h>
#include <covent/gather.h>
#include <event2/bufferevent_ssl.h>
#include <fmt/format.h>

using namespace covent::http;

class Client::HTTPSession : public covent::Session {
public:
    using covent::Session::Session;

    std::unique_ptr<covent::http::Message> message;
    covent::future<bool> ready;

    covent::task<unsigned long> process(std::string_view data) override {
        if (!message) {
            m_log->debug("Data arrived before I was ready?");
            co_return 0;
        }
        auto tmp = message->process(data);
        if (message->complete) {
            ready.resolve(true);
        }
        co_return tmp;
    }
};

Client::Client(Service & service, URI const & uri)
    : m_loop(covent::Loop::thread_loop()), m_service(service), m_uri(uri) {
    m_log = Application::application().logger("HTTP::Client");
}

Client::Client(Service & service, std::string_view uri)
    : m_loop(covent::Loop::thread_loop()), m_service(service), m_uri(uri) {
    m_log = Application::application().logger("HTTP::Client");
}

Request Client::request(Method method, URI const & uri, std::optional<std::string> body) {
    return Request(*this, method, uri);
}

Request Client::request(Method method, std::string_view uri, std::optional<std::string> body) {
    return Request(*this, method, URI(uri));
}

covent::task<std::shared_ptr<Client::HTTPSession>> Client::connect(covent::dns::answers::Address address, URI const & uri) const {
    bool ssl = false;
    if (uri.scheme == "https") {
        ssl = true;
    }
    bool success = false;
    auto session = std::make_shared<HTTPSession>(m_loop); // Don't add it to the loop yet!
    auto & tls_context = m_service.entry(uri.host).tls_context();
    auto & validator = m_service.entry(uri.host).validator();
    for (auto & s : address.addr) {
        try {
            covent::sockaddr_cast<AF_INET6>(&s)->sin6_port = htons(uri.port.value());
            co_await session->connect(&s);
            if (ssl) {
                co_await session->ssl(tls_context.instantiate(true, uri.host), true);
                co_await validator.verify_tls(session->ssl(), uri.host);
            }
            success = true;
            break;
        } catch (std::runtime_error & e) {
            m_log->info("Error {}, ignoring", e.what());
            // Loop around and try again.
        }
    }
    if (!success) {
        session.reset();
    } else {
        m_loop.add(session);
    }
    co_return session;
}

covent::task<std::unique_ptr<Response>> Client::send(Request const &r) {
    std::shared_ptr<Client::HTTPSession> session;
    std::list<task<std::shared_ptr<HTTPSession>>> attempts;

    auto const & uri = r.uri();
    auto & resolver = m_service.entry(uri.host).resolver();
    auto [addr_v4, addr_v6] = co_await gather(resolver.address_v4(uri.host), resolver.address_v6(uri.host));

    if (!addr_v6.addr.empty()) {
        attempts.emplace_back(connect(addr_v6, uri));
        try {
            session = co_await race(attempts, 0.1, [](auto const & x) { return !!x; });
        } catch (covent::race_timeout &) { // NOSONAR
            // Carry on!
        }
    }
    if (!session) {
        if (!attempts.empty() && attempts.rbegin()->done()) {
            attempts.clear();
        }
        if (!addr_v4.addr.empty()) {
            attempts.emplace_back(connect(addr_v4, uri));
        }
        try {
            session = co_await race(attempts, 5, [](auto const & x) { return !!x; });
        } catch (covent::race_timeout &) { // NOSONAR
            // Carry on!
        }
    }
    if (!session) {
        throw std::runtime_error("Connection failed");
    }
    session->message = std::make_unique<Message>();
    r.send(*session);
    co_await session->ready;
    co_return std::make_unique<Response>(std::move(session->message));
}
