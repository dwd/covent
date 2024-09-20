//
// Created by dwd on 8/26/24.
//

#include <covent/core.h>
#include <covent/loop.h>
#include <event2/bufferevent.h>
#include <event2/event.h>
#include <event2/buffer.h>

namespace{
    covent::Session::id_type next_id = 0;
    void bev_read_cb(bufferevent * bev, void * arg) {
        auto * session = reinterpret_cast<covent::Session *>(arg);
        session->read_cb(bev);
    }
    void bev_write_cb(bufferevent * bev, void * arg) {
        auto * session = reinterpret_cast<covent::Session *>(arg);
        session->write_cb(bev);
    }
    void bev_event_cb(bufferevent * bev, short flags, void * arg) {
        auto *session = reinterpret_cast<covent::Session *>(arg);
        session->event_cb(bev, flags);
    }
}

covent::Session::Session(Loop & loop): m_loop(loop), m_id(next_id++) {
}

covent::Session::Session(covent::Loop &loop, int sock): m_loop(loop), m_id(next_id++) {
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

covent::task<void> covent::Session::connect(struct sockaddr * addr, size_t addrlen) {
    if (m_base_buf) throw std::logic_error("Already connected?");
    m_base_buf = bufferevent_socket_new(m_loop.event_base(), -1, BEV_OPT_CLOSE_ON_FREE);
    m_top = m_base_buf;
    if (0 > bufferevent_socket_connect(m_base_buf, addr, addrlen)) {
        m_base_buf = m_top = nullptr;
        throw std::runtime_error("Some kind of connection failure");
    }
    bufferevent_setcb(m_top, bev_read_cb, bev_write_cb, bev_event_cb, this);
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
    co_await written;
    co_return;
}

void covent::Session::used(size_t len) {
    evbuffer_drain(bufferevent_get_input(m_top), len);
}

void covent::Session::read_cb(struct bufferevent *bev) {
    if (m_processor.has_value()) {
        // Wait until it's done.
    } else {
        // Create and start the process task.
        // We'll try first using whatever contiguous data we have to hand:
        size_t len;
        struct evbuffer *buf;
        while ((len = evbuffer_get_contiguous_space(buf = bufferevent_get_input(m_top))) > 0) {
            if (len == 0) break;
            m_processor.emplace(process({reinterpret_cast<char *>(evbuffer_pullup(buf, len)), len}));
            if (m_processor->start()) {
                auto used_any = m_processor->get();
                m_processor.reset();
                if (!used_any) break;
            } else {
                return;
            }
            // TODO : Check closed
        }
        while ((len = evbuffer_get_length(buf = bufferevent_get_input(m_top))) > 0) {
            m_processor.emplace(process({reinterpret_cast<char *>(evbuffer_pullup(buf, -1)), len}));
            if (m_processor->start()) {
                auto used_any = m_processor->get();
                m_processor.reset();
                if (!used_any) break;
            } else {
                return;
            }
            // TODO : Check closed
        }
    }
}

void covent::Session::write_cb(struct bufferevent *bev) {
    this->written(true);
    // Not actually sure!
}

void covent::Session::event_cb(struct bufferevent *bev, short flags) {
    if (flags & BEV_EVENT_CONNECTED) {
        this->connected(true);
    }
    // Process flags (close, etc).
}
