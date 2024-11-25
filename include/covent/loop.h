//
// Created by dwd on 8/24/24.
//

#ifndef COVENT_LOOP_H
#define COVENT_LOOP_H

#include <covent/base.h>
#include <covent/core.h>
#include <sys/socket.h>
#include <vector>
#include <memory>
#include <map>
#include <coroutine>
#include <functional>
#include <optional>
#include <mutex>
#include <set>
#include <event2/util.h>

struct event_base;

template<>
struct std::less<struct timeval> {
    constexpr bool operator()(struct timeval const & t1, struct timeval const & t2) const {
        if (t1.tv_sec == t2.tv_sec) return t1.tv_usec < t2.tv_usec;
        return t1.tv_sec < t2.tv_sec;
    }
};

namespace covent {
    template<typename Id>
    struct Compare {
        using is_transparent = void;
        bool operator()(Id::id_type id, std::shared_ptr<Id> const & o2) const {
            return id < o2->id();
        }
        bool operator()(std::shared_ptr<Id> const & o1, Id::id_type id) const {
            return o1->id() < id;
        }
        bool operator()(std::shared_ptr<Id> const & o1, std::shared_ptr<Id> const & o2) const {
            return o1->id() < o2->id();
        }
        bool operator()(Id::id_type i1, Id::id_type i2) const {
            return i1 < i2;
        }
    };

    class Loop {
    public:
        Loop();
        ~Loop();
        static Loop & thread_loop();
        static Loop & main_loop();

        void run(); // Run forever (or at least, until it's shutdown).

        void run_once(bool block); // Run a single cycle, including deferred calls.

        void run_until(std::function<bool()> const&);
        void run_until_complete(); // Run single cycles until no sessions or deferred calls exist. A listening session will never end!
        void run_until_complete(Session const &); // Run single cycles until this session is closed.
        template<typename V>
        V run_task(task<V> && task) {
            task.start();
            this->run_until([&task]() { return task.done(); });
            if (task.done()) {
                return task.get();
            } else {
                // Loop has been killed via shutdown before the task could complete.
                throw covent::covent_runtime_error("Loop shutdown before task completed");
            }
        }

        void shutdown();

        std::shared_ptr<Session> add(std::shared_ptr<Session> const &);
        std::shared_ptr<Session> session(Session::id_type id) const;
        void remove(Session & session);
        void remove(std::shared_ptr<Session> const & session);

        void listen(ListenerBase &);

        void defer(std::function<void()> && fn);
        void defer(std::function<void()> && fn, long seconds);
        void defer(std::function<void()> && fn, double seconds);
        void defer(std::function<void()> && fn, struct timeval seconds);

        struct event_base * event_base() {
            return m_event_base.get();
        }

    private:
        bool set_next_break();

        std::unique_ptr<struct event_base, std::function<void(struct event_base *)>> m_event_base;
        std::set<std::shared_ptr<Session>, Compare<Session>> m_sessions;
        std::recursive_mutex m_scheduler_mutex;
        std::multimap<struct timeval, std::function<void()>> m_pending_actions;
        bool m_shutdown = false;
    };
}

#endif //COVENT_LOOP_H
