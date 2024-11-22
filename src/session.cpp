//
// Created by dwd on 8/26/24.
//

#include <covent/core.h>
#include <covent/loop.h>
#include <event2/bufferevent.h>
#include <event2/event.h>
#include <event2/buffer.h>
#include <iostream>
#include <event2/bufferevent_ssl.h>

namespace{
    covent::Session::id_type next_id = 0;
    void bev_read_cb(bufferevent * bev, void * arg) {
        auto * session = static_cast<covent::Session *>(arg);
        session->read_cb(bev);
    }
    void bev_write_cb(bufferevent * bev, void * arg) {
        auto * session = static_cast<covent::Session *>(arg);
        session->write_cb(bev);
    }
    void bev_event_cb(bufferevent * bev, short flags, void * arg) {
        auto *session = static_cast<covent::Session *>(arg);
        session->event_cb(bev, flags);
    }
}

covent::Session::Session(Loop & loop): m_id(next_id++), m_loop(loop) {
}

covent::Session::Session(covent::Loop &loop, int sock): m_id(next_id++), m_loop(loop) {
    m_base_buf = bufferevent_socket_new(m_loop.event_base(), sock, BEV_OPT_CLOSE_ON_FREE);
    m_top = m_base_buf;
    bufferevent_setcb(m_top, bev_read_cb, bev_write_cb, bev_event_cb, this);
    bufferevent_enable(m_top, EV_READ | EV_WRITE);
}

covent::Session::~Session() {
    if (m_top) {
        bufferevent_flush(m_top, EV_WRITE, BEV_FINISHED);
    }
    if (m_base_buf) {
        bufferevent_free(m_base_buf);
    }
}

covent::task<void> covent::Session::connect(const struct sockaddr * addr, size_t addrlen) {
    if (m_base_buf) throw covent_logic_error("Already connected?");
    m_base_buf = bufferevent_socket_new(m_loop.event_base(), -1, BEV_OPT_CLOSE_ON_FREE);
    m_top = m_base_buf;
    std::cout << "Connecting to [" << address_tostring(addr) << "]:" << address_toport(addr) << std::endl;
    bufferevent_setcb(m_top, bev_read_cb, bev_write_cb, bev_event_cb, this);
    if (0 > bufferevent_socket_connect(m_base_buf, addr, static_cast<int>(addrlen))) {
        m_base_buf = m_top = nullptr;
        throw covent_runtime_error(std::strerror(errno));
    }
    bufferevent_enable(m_top, EV_READ | EV_WRITE);
    co_await connected;
    co_return;
}

void covent::Session::write(std::string_view data) {
    auto buf = bufferevent_get_output(m_top);
    evbuffer_add(buf, data.data(), data.length());
}

covent::task<void> covent::Session::flush(std::string_view data) {
    if (!data.empty()) write(data);
    bufferevent_flush(m_top, EV_WRITE, BEV_FLUSH);
    co_await written;
    co_return;
}

void covent::Session::used(size_t len) {
    evbuffer_drain(bufferevent_get_input(m_top), len);
}

void covent::Session::read_cb(struct bufferevent *) {
    if (m_processor.has_value()) {
        // Wait until it's done.
        if (m_processor->done()) {
            m_processor.reset();
        } else {
            return;
        }
    }
    // Create and start the process task.
    // We'll try first using whatever contiguous data we have to hand:
    size_t len;
    struct evbuffer *buf = bufferevent_get_input(m_top);
    while ((len = evbuffer_get_contiguous_space(buf)) > 0) {
        m_processor.emplace(process({reinterpret_cast<char *>(evbuffer_pullup(buf, static_cast<ssize_t>(len))), len}));
        if (!m_processor->start()) return;
        auto used_any = m_processor->get();
        m_processor.reset();
        if (!used_any) break;
        // TODO : Check closed
    }
    while ((len = evbuffer_get_length(buf)) > 0) {
        m_processor.emplace(process({reinterpret_cast<char *>(evbuffer_pullup(buf, -1)), len}));
        if (!m_processor->start()) return;
        auto used_any = m_processor->get();
        m_processor.reset();
        if (!used_any) break;
        // TODO : Check closed
    }
}

void covent::Session::write_cb(struct bufferevent *) {
    this->written.resolve();
    // Not actually sure!
}

void covent::Session::event_cb(struct bufferevent *, short flags) {
    if (flags & BEV_EVENT_CONNECTED) {
        this->connected.resolve();
    }
    // Process flags (close, etc).
}

SSL * covent::Session::ssl() const {
    return bufferevent_openssl_get_ssl(m_top);
}

void covent::Session::ssl(SSL *s, bool connecting) {
    m_top = bufferevent_openssl_filter_new(bufferevent_get_base(m_top), m_top, s, connecting ? BUFFEREVENT_SSL_CONNECTING : BUFFEREVENT_SSL_ACCEPTING, BEV_OPT_CLOSE_ON_FREE);
}