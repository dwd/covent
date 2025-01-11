#include "covent/loop.h"
#include "covent/core.h"
#include "covent/dns.h"
#include "covent/pkix.h"
#include <event2/event.h>
#include <event2/thread.h>
#include <event2/listener.h>
#include <list>

namespace {
    thread_local covent::Loop * s_thread_loop = nullptr;
    covent::Loop * s_main_loop = nullptr;
}

covent::Loop::Loop() {
    evthread_use_pthreads();
    m_event_base = std::unique_ptr<struct event_base, std::function<void(struct event_base *)>>(event_base_new(), [](struct event_base * b){
        if (b) event_base_free(b);
    });
    if (!s_main_loop) s_main_loop = this;
    if (!s_thread_loop) s_thread_loop = this;
}

covent::Loop &covent::Loop::thread_loop() {
    if (s_thread_loop) {
        return *s_thread_loop;
    }
    if (s_main_loop) {
        return *s_main_loop;
    }
    throw covent_logic_error("No loop found for thread (or main)");
}

covent::Loop &covent::Loop::main_loop() {
    if (s_main_loop) {
        return *s_main_loop;
    }
    throw covent_logic_error("No loop found for main");
}

void covent::Loop::run_once(bool block) {
    auto flags = EVLOOP_ONCE | EVLOOP_NO_EXIT_ON_EMPTY;
    block = !set_next_break() && block;
    if (!block) flags |= EVLOOP_NONBLOCK;
    if (m_shutdown) return;
    event_base_loop(m_event_base.get(), flags);
    if (m_shutdown) return;
    for (;;) {
        std::list<std::function<void()>> run_now;
        {
            std::scoped_lock<std::recursive_mutex> l_(m_scheduler_mutex);
            struct timeval now;
            gettimeofday(&now, nullptr);
            while (!m_pending_actions.empty()) {
                auto & next = m_pending_actions.begin()->first;
                if (std::less<struct timeval>{}.operator()(next, now)) {
                    run_now.emplace_back(std::move(m_pending_actions.begin()->second));
                    m_pending_actions.erase(m_pending_actions.begin());
                } else {
                    break;
                }
            }
        }
        if (run_now.empty()) break;
        for (auto & fn : run_now) {
            fn();
        }
    }
}

void covent::Loop::run_until(std::function<bool()> const & fn) {
    while (!m_shutdown) {
        if (fn()) break;
        run_once(true);
    }
}

void covent::Loop::run_until_complete() {
    run_until([this](){ return m_sessions.empty() && m_pending_actions.empty(); });
}

void covent::Loop::run_until_complete(Session const & session) {
    auto session_id = session.id();
    run_until([this, session_id]() {
        return m_sessions.contains(session_id);
    });
};

void covent::Loop::run() {
    while(!m_shutdown) {
        run_once(true);
    }
}

void covent::Loop::shutdown() {
    m_shutdown = true;
    struct timeval tv{0,0};
    event_base_loopexit(m_event_base.get(), &tv);
}

covent::Loop::~Loop() {
    if (s_main_loop == this) s_main_loop = nullptr;
    if (s_thread_loop == this) s_thread_loop = nullptr;
}


bool covent::Loop::set_next_break() {
    struct timeval now;
    gettimeofday(&now, nullptr);
    if (m_pending_actions.empty()) return false;
    struct timeval t = m_pending_actions.begin()->first;
    if (t.tv_usec <= now.tv_usec) {
        if (t.tv_sec <= now.tv_sec) {
            event_base_loopbreak(m_event_base.get());
            return true;
        }
        t.tv_sec -= 1;
        t.tv_usec += 1000000;
    }
    if (t.tv_sec < now.tv_sec) {
        event_base_loopbreak(m_event_base.get());
        return true;
    }
    t.tv_sec -= now.tv_sec;
    t.tv_usec -= now.tv_usec;
    event_base_loopexit(m_event_base.get(), &t);
    return false;
}

void covent::Loop::defer(std::function<void()> &&fn, struct timeval seconds) {
    std::scoped_lock<std::recursive_mutex> l_(m_scheduler_mutex);
    struct timeval now;
    gettimeofday(&now, nullptr);
    struct timeval when = now;
    when.tv_sec += seconds.tv_sec;
    when.tv_usec += seconds.tv_usec;
    if (when.tv_usec >= 1000000) {
        when.tv_sec += 1;
        when.tv_usec -= 1000000;
    }
    m_pending_actions.emplace(when, std::move(fn));
    if (seconds.tv_sec == 0 && seconds.tv_usec == 0) {
        event_base_loopbreak(m_event_base.get());
    } else {
        set_next_break();
    }
}

void covent::Loop::defer(std::function<void()> &&fn, double seconds) {
    struct timeval when{
        .tv_sec = static_cast<time_t>(seconds),
        .tv_usec = static_cast<suseconds_t>(seconds * 1000000) % 1000000
    };
    this->defer(std::move(fn), when);
}

void covent::Loop::defer(std::function<void()> &&fn, long seconds) {
    this->defer(std::move(fn), {seconds, 0});
}

void covent::Loop::defer(std::function<void()> &&fn) {
    this->defer(std::move(fn), {0, 0});
}

void covent::Loop::listen(ListenerBase & listener) {
    listener.listen(*this);
}

std::shared_ptr<covent::Session> covent::Loop::add(std::shared_ptr<Session> const & optr) {
    m_sessions.emplace(optr);
    return optr;
}

std::shared_ptr<covent::Session> covent::Loop::session(Session::id_type id) const {
    auto result = m_sessions.find(id);
    if (result == m_sessions.end()) {
        throw std::runtime_error("Session not found");
    }
    return *result;
}

void covent::Loop::remove(covent::Session &sess) {
    auto ptr = session(sess.id());
    remove(ptr);
}

void covent::Loop::remove(std::shared_ptr<Session> const & sess) {
    m_sessions.erase(sess);
}

covent::dns::Resolver &covent::Loop::default_resolver() {
    if (!m_default_resolver) {
        m_default_resolver = std::make_unique<covent::dns::Resolver>(false, false, std::optional<std::string>{});
    }
    return *m_default_resolver;
}

covent::pkix::TLSContext &covent::Loop::default_tls_context() {
    if (!m_default_tls_context) {
        m_default_tls_context = std::make_unique<pkix::TLSContext>(true, false, "default");
    }
    return *m_default_tls_context;
}

covent::pkix::PKIXValidator &covent::Loop::default_pkix_validator() {
    if (!m_default_pkix_validator) {
        m_default_pkix_validator = std::make_unique<pkix::PKIXValidator>();
    }
    return *m_default_pkix_validator;
}