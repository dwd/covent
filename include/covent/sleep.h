//
// Created by dwd on 11/24/24.
//

#ifndef COVENT_SLEEP_H
#define COVENT_SLEEP_H

#include <covent/covent.h>
#include <covent/coroutine.h>

namespace covent {
    template<typename Time>
    struct sleep {
        static constexpr bool no_loop_resume = true;
        Time m_time;

        explicit sleep(Time t) : m_time(t) {}

        static bool await_ready() { return false; }
        static void await_resume() {}
        template<typename A, typename L>
        void await_suspend(std::coroutine_handle<detail::wrapped_promise<A,L>> p) const {
            std::weak_ptr<bool> liveness = p.promise().liveness;
            p.promise().loop->defer([p, liveness]() {
                auto alive = liveness.lock();
                if (alive) {
                    p.resume();
                }
            }, m_time);
        }
        template<typename A>
        void await_suspend(std::coroutine_handle<detail::promise<A>> p) const {
            std::weak_ptr<bool> liveness = p.promise().liveness;
            p.promise().loop->defer([p, liveness]() {
                auto alive = liveness.lock();
                if (alive) {
                    p.resume();
                }
            }, m_time);
        }
        void await_suspend(std::coroutine_handle<> p) const {
            // Also here?
            covent::Loop::thread_loop().defer([p]() {
                p.resume();
            }, m_time);
        }
        auto & operator co_await () const {
            return *this;
        }
    };
}

#endif //COVENT_SLEEP_H
