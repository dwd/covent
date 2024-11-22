//
// Created by dwd on 11/12/24.
//

#ifndef COVENT_FUTURE_H
#define COVENT_FUTURE_H

#include <optional>
#include <utility>
#include <coroutine>

namespace covent {
    template<typename V>
    class future {
    public:
        future() = default;
        future(future const &) = delete;

        template<typename ...Args>
        void resolve(Args && ...v) {
            m_value.emplace(std::forward<Args>(v)...);
            gogogo();
        }
        void exception(std::exception_ptr && e) {
            m_except = std::move(e);
            gogogo();
        }
        auto & operator co_await() const {
            return *this;
        }

        bool await_ready() const {
            return m_value.has_value() || m_except;
        }

        std::coroutine_handle<> await_suspend(std::coroutine_handle<> awaiting_coroutine) const {
            if (await_ready()) {
                return awaiting_coroutine;
            }
            m_coro = awaiting_coroutine;
            return std::noop_coroutine();
        }

        V await_resume() const {
            if (m_except) {
                std::rethrow_exception(m_except);
            }
            return std::move(*m_value);
        }
    private:
        void gogogo() {
            m_coro.resume();
        }
        std::optional<V> m_value;
        std::exception_ptr m_except;
        mutable std::coroutine_handle<> m_coro = std::noop_coroutine();
    };
    template<>
    class future<void> : public future<bool> {
    public:
        using future<bool>::future;
        void resolve() {
            future<bool>::resolve(true);
        }
        void await_resume() const {
            future<bool>::await_resume();
        }
        auto & operator co_await() const {
            return *this;
        }
    };
}

#endif //COVENT_FUTURE_H
