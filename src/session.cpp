//
// Created by dwd on 8/26/24.
//

#include <covent/core.h>
#include <covent/loop.h>
#include <event2/bufferevent.h>
#include <event2/event.h>

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
    m_buf = std::unique_ptr<struct bufferevent, std::function<void(struct bufferevent *)>>(bufferevent_socket_new(m_loop.event_base(), sock, BEV_OPT_CLOSE_ON_FREE|BEV_OPT_DEFER_CALLBACKS), [](struct bufferevent * b){
        if (b) bufferevent_free(b);
    });
    m_top = m_buf.get();
    bufferevent_setcb(m_buf.get(), bev_read_cb, bev_write_cb, bev_event_cb, this);
    bufferevent_enable(m_buf.get(), EV_READ | EV_WRITE);
}

covent::task<void> covent::Session::connect(struct sockaddr * addr) {
    co_return;
}

void covent::Session::write(std::string_view data) {
    return;
}