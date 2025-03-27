//
// Created by dwd on 8/26/24.
//

#include <covent/app.h>
#include <covent/core.h>
#include <covent/loop.h>
#include <event2/bufferevent.h>
#include <event2/event.h>
#include <event2/buffer.h>
#include <covent/http.h>
#include <event2/bufferevent_ssl.h>
#include <openssl/err.h>

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
    m_log = Application::application().logger("Session");
}

covent::Session::Session(covent::Loop &loop, int sock, ListenerBase &): m_id(next_id++), m_loop(loop) {
    m_log = Application::application().logger("Session");
    m_top = bufferevent_socket_new(m_loop.event_base(), sock, BEV_OPT_CLOSE_ON_FREE);
    bufferevent_setcb(m_top, bev_read_cb, bev_write_cb, bev_event_cb, this);
    bufferevent_enable(m_top, EV_READ | EV_WRITE);
}

covent::Session::~Session() {
    if (m_top) {
        bufferevent_flush(m_top, EV_WRITE, BEV_FINISHED);
        bufferevent_free(m_top);
    }
}

covent::task<void> covent::Session::connect(const struct sockaddr * addr, size_t addrlen) {
    if (m_top) throw covent_logic_error("Already connected?");
    m_top = bufferevent_socket_new(m_loop.event_base(), -1, BEV_OPT_CLOSE_ON_FREE);
    m_log->info("Connecting to [{}]:{}", address_tostring(addr), address_toport(addr));
    bufferevent_setcb(m_top, bev_read_cb, bev_write_cb, bev_event_cb, this);
    if (0 > bufferevent_socket_connect(m_top, addr, static_cast<int>(addrlen))) {
        m_top = nullptr;
        m_log->info("Connect: {}", std::strerror(errno));
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

void covent::Session::processing_complete() {
    auto self = loop().session(id());
    try {
        auto used_octets = m_processor->get();
        if (used_octets) used(used_octets);
        if (m_closing) {
            m_log->debug("Retry close");
            close();
        }
    } catch(std::exception & e) {
        // Unhandled exception.
        m_log->debug("Deferred exception caught: {}", e.what());
        close();
    }
}

void covent::Session::read_cb(struct bufferevent *) {
    auto self = loop().session(id());
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
    m_log->trace("Contiguous data mode");
    while ((len = evbuffer_get_contiguous_space(buf)) > 0) {
        try {
            m_processor.emplace(process({reinterpret_cast<char *>(evbuffer_pullup(buf, static_cast<ssize_t>(len))), len}));
            if (!m_processor->start()) {
                m_processor->on_completed(this, &Session::processing_complete);
                return;
            }
            auto used_octets = m_processor->get();
            m_processor.reset();
            m_log->trace("Immediate/contiguous: used {} octets", used_octets);
            if (!used_octets) break;
            used(used_octets);
        } catch (std::exception & e) {
            m_log->debug("Immediate/contiguous: exception caught: {}", e.what());
            m_processor.reset();
            close();
            return;
        }
        if (!m_top) return;
        buf = bufferevent_get_input(m_top);
        // TODO : Check closed
    }
    if (!m_top) return;
    buf = bufferevent_get_input(m_top);
    if (evbuffer_get_contiguous_space(buf) == evbuffer_get_length(buf)) {
        // No need to retry.
        m_log->trace("No data left");
        return;
    }
    m_log->trace("All data mode");
    while ((len = evbuffer_get_length(buf)) > 0) {
        try {
            m_processor.emplace(process({reinterpret_cast<char *>(evbuffer_pullup(buf, -1)), len}));
            if (!m_processor->start()) {
                m_processor->on_completed(this, &Session::processing_complete);
                return;
            }
            auto used_octets = m_processor->get();
            m_processor.reset();
            m_log->trace("Immediate/pull-up: used {} octets", used_octets);
            if (!used_octets) break;
            used(used_octets);
        } catch(std::exception& e) {
            m_log->debug("Immediate/pull-up: exception caught: {}", e.what());
            m_processor.reset();
            close();
            return;
        }
        if (!m_top) return;
        buf = bufferevent_get_input(m_top);
        // TODO : Check closed
    }
}

void covent::Session::write_cb(struct bufferevent *) {
    this->written.emit();
    if (m_closing) {
        close();
    }
    // Not actually sure!
}

void covent::Session::event_cb(struct bufferevent * buf, short flags) {
    if (buf != m_top) {
        return;
    }
    if (flags & BEV_EVENT_CONNECTED) {
        m_log->debug("Connected");
        this->connected.emit();
    }
    if (flags & BEV_EVENT_EOF) {
        m_log->debug("Disconnected");
        this->closed();
    }
    if (flags & BEV_EVENT_ERROR) {
        m_log->error("Error: {}", strerror(EVUTIL_SOCKET_ERROR()));
        while (unsigned long ssl_err = bufferevent_get_openssl_error(buf)) {
            std::array<char, 1024> err_buffer = {};
            ERR_error_string_n(ssl_err, err_buffer.data(), 1024);
            m_log->error(" - OpenSSL error: {}", err_buffer.data());
        }
        this->closed();
    }
    if (flags & BEV_EVENT_TIMEOUT) {
        m_log->warn("Timeout");
        this->closed();
    }
    // Process flags (close, etc).
}

SSL * covent::Session::ssl() const {
    return bufferevent_openssl_get_ssl(m_top);
}

sigslot::signal<> & covent::Session::ssl(SSL *s, bool connecting) {
    m_top = bufferevent_openssl_filter_new(bufferevent_get_base(m_top), m_top, s, connecting ? BUFFEREVENT_SSL_CONNECTING : BUFFEREVENT_SSL_ACCEPTING, BEV_OPT_CLOSE_ON_FREE);
    bufferevent_setcb(m_top, bev_read_cb, bev_write_cb, bev_event_cb, this);
    bufferevent_enable(m_top, EV_READ | EV_WRITE);
    return connected;
}

void covent::Session::close() {
    m_closing = true;
    if (m_processor.has_value() && !m_processor->done()) {
        return;
    }
    if (m_top) {
        bufferevent_flush(m_top, EV_WRITE, BEV_FINISHED);
        auto * buf = bufferevent_get_output(m_top);
        if (evbuffer_get_length(buf) > 0) {
            return;
        }
        bufferevent_free(m_top);
        m_top = nullptr;
    }
    m_loop.defer([session = this, loop = &m_loop]() {
        loop->remove(*session);
    });
}

bufferevent * covent::Session::eject() {
    auto * ret = m_top;
    m_top = nullptr;
    bufferevent_setcb(ret, nullptr, nullptr, nullptr, nullptr);
    bufferevent_disable(ret, EV_READ|EV_WRITE);
    m_loop.remove(*this);
    return ret;
}
