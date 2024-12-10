//
// Created by dwd on 12/6/24.
//
#include <covent/http.h>
#include <covent/loop.h>
#include <event2/buffer.h>
#include <event2/http.h>
#include <algorithm>
#include <event2/bufferevent_ssl.h>

using namespace covent::http;

namespace {
	auto * bufferevent_cb(struct event_base * base, void * arg) {
        auto * server = static_cast<Server *>(arg);
	    return server->get_buffer_event(base);
	}

    void request_cb(struct evhttp_request *req, void * arg) {
	    auto * server = static_cast<Server *>(arg);
	    return server->request_handler(req);
	}
}

Endpoint::Endpoint(std::string const & path): m_path(path), m_handler([](evhttp_request * req) -> covent::task<int> {
	evhttp_send_reply(req, 404, "Not found", nullptr);
	co_return 404;
}) {}


void Endpoint::add(std::unique_ptr<Endpoint> && endpoint) {
	m_endpoints[endpoint->path()] = std::move(endpoint);
}

covent::task<int> Endpoint::handler_low(evhttp_request * req) {
	auto * uri = evhttp_request_get_evhttp_uri(req);
	auto * path = evhttp_uri_get_path(uri);
	if (path == m_path) {
		co_return co_await this->handler(req);
	}

	auto ret = m_endpoints.find(evhttp_uri_get_path(uri));
	if (ret == m_endpoints.end()) {
		evhttp_send_reply(req, 404, "Not found", nullptr);
		co_return 404;
	} else {
		co_return co_await ret->second->handler(req);
	}
}

covent::task<int> Endpoint::handler(evhttp_request * req) {
	return m_handler(req);
}

Server::Server(unsigned short port, covent::pkix::TLSContext &tls_context) : m_tls_context(tls_context) {
	auto & loop = covent::Loop::thread_loop();
    m_server = evhttp_new(loop.event_base());
    evhttp_bind_socket(m_server, "0.0.0.0", port);
    evhttp_set_bevcb(m_server, bufferevent_cb, this);
    evhttp_set_gencb(m_server, request_cb, this);
}

void Server::add(std::unique_ptr<Endpoint> && endpoint) {
    if (endpoint->path() == "/") {
        if (m_root) throw std::runtime_error("Already have a root endpoint");
        m_root = std::move(endpoint);
    } else {
        if (!m_root) throw std::runtime_error("No root endpoint");
        m_root->add(std::move(endpoint));
    }
}

struct bufferevent *Server::get_buffer_event(struct event_base * base) {
	if (m_tls_context.enabled()) {
		return bufferevent_openssl_socket_new(base, -1, m_tls_context.instantiate(false, "http"), BUFFEREVENT_SSL_ACCEPTING,
											  BEV_OPT_CLOSE_ON_FREE | BEV_OPT_DEFER_CALLBACKS);
	} else {
		return bufferevent_socket_new(base, -1, BEV_OPT_CLOSE_ON_FREE | BEV_OPT_DEFER_CALLBACKS);
	}
}


void Server::request_handler(struct evhttp_request *req) {
	const auto ret = std::ranges::remove_if(m_in_flight, [](auto & task) {
		return task.done();
	});
	m_in_flight.erase(ret.begin(), ret.end());
	m_in_flight.emplace_back(handler_low(req));
	if (m_in_flight.rbegin()->start()) {
		m_in_flight.pop_back();
	}
}

covent::task<void> Server::handler_low(struct evhttp_request *req) {
	try {
		if (!m_root) throw std::runtime_error("Missing root endpoint");
		auto response_code = co_await m_root->handler_low(req);
		if (response_code == 0) throw std::runtime_error("Unknown error");
	} catch (std::runtime_error & e) {
		auto *reply = evbuffer_new();
		evbuffer_add_printf(reply, "Runtime Exception: %s", e.what());
		evhttp_send_reply(req, HTTP_INTERNAL, "Unexpected exception", reply);
		evbuffer_free(reply);
	}
}

Server::~Server() {
	evhttp_free(m_server);
}