//
// Created by dwd on 12/6/24.
//
#include <covent/http.h>
#include <covent/loop.h>
#include <event2/buffer.h>
#include <event2/http.h>
#include <algorithm>
#include <utility>
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

covent::task<void> Middleware::handle(evhttp_request * req) {
	return m_handler(req);
}

Endpoint::Endpoint(std::string  path): m_path(std::move(path)), m_handler([](evhttp_request *) -> covent::task<int> {
	throw exception::not_found();
}) {}

void Endpoint::add(std::unique_ptr<Endpoint> && endpoint) {
	m_endpoints[endpoint->path()] = std::move(endpoint);
}

void Endpoint::add(std::unique_ptr<Middleware> && endpoint) {
	m_middleware.emplace_back(std::move(endpoint));
}

covent::task<int> Endpoint::handler_low(evhttp_request * req) {
	for (auto const & middleware : m_middleware) {
		co_await middleware->handle(req);
	}
	auto * uri = evhttp_request_get_evhttp_uri(req);
	auto * path = evhttp_uri_get_path(uri);
	if (path == m_path) {
		co_return co_await this->handler(req);
	}

	auto ret = m_endpoints.find(evhttp_uri_get_path(uri));
	if (ret == m_endpoints.end()) {
		throw exception::not_found();
	} else {
		co_return co_await ret->second->handler_low(req);
	}
}

covent::task<int> Endpoint::handler(evhttp_request * req) {
	return m_handler(req);
}

Server::Server(unsigned short port, bool tls) {
	auto & loop = covent::Loop::thread_loop();
	m_service = std::make_unique<Service>();
	if (!tls) {
		auto & entry = m_service->add("");
		entry.make_tls_context(false, false, "");
	}
    m_server = evhttp_new(loop.event_base());
    if (0 != evhttp_bind_socket(m_server, "::", port)) {
	    throw std::runtime_error(std::strerror(errno));
    }
    evhttp_set_bevcb(m_server, bufferevent_cb, this);
    evhttp_set_gencb(m_server, request_cb, this);
}

void Server::add(std::unique_ptr<Endpoint> && endpoint) {
    if (endpoint->path() == "/") {
        if (m_root) throw exception::base("Already have a root endpoint");
        m_root = std::move(endpoint);
    } else {
        if (!m_root) throw exception::base("No root endpoint");
        m_root->add(std::move(endpoint));
    }
}

struct bufferevent *Server::get_buffer_event(struct event_base * base) {
	auto & tls_context = m_service->entry("").tls_context();
	if (tls_context.enabled()) {
		return bufferevent_openssl_socket_new(base, -1, tls_context.instantiate(false, "http"), BUFFEREVENT_SSL_ACCEPTING,
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
	try {
		if (m_in_flight.rbegin()->start()) {
			m_in_flight.pop_back();
		}
	} catch (exception::http_status & e) {
		evhttp_send_reply(req, e.status(), e.what(), nullptr);
	} catch (std::runtime_error & e) {
		auto *reply = evbuffer_new();
		evbuffer_add_printf(reply, "Runtime Exception: %s", e.what());
		evhttp_send_reply(req, HTTP_INTERNAL, "Unexpected exception", reply);
		evbuffer_free(reply);
	}
}

covent::task<void> Server::handler_low(struct evhttp_request *req) {
	try {
		if (!m_root) throw exception::base("Missing root endpoint");
		auto response_code = co_await m_root->handler_low(req);
		if (response_code == 0) throw exception::base("Unknown error");
	} catch (exception::http_status & e) {
		evhttp_send_reply(req, e.status(), e.what(), nullptr);
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