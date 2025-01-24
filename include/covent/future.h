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

        bool await_suspend(std::coroutine_handle<> awaiting_coroutine) const {
            if (await_ready()) {
                return false;
            }
            m_liveness = m_dummy;
            m_coro = awaiting_coroutine;
            return true;
        }

        template<typename A, typename B>
        bool await_suspend(std::coroutine_handle<covent::detail::wrapped_promise<A, B>> awaiting_coroutine) const {
            if (await_ready()) {
                return false;
            }
            m_liveness = awaiting_coroutine.promise().liveness;
            m_coro = awaiting_coroutine;
            return true;
        }

        template<typename A, typename B>
        bool await_suspend(std::coroutine_handle<covent::detail::promise<A, B>> awaiting_coroutine) const {
            if (await_ready()) {
                return false;
            }
            m_liveness = awaiting_coroutine.promise().liveness;
            m_coro = awaiting_coroutine;
            return true;
        }

        V await_resume() const {
            if (m_except) {
                std::rethrow_exception(m_except);
            }
            return std::move(*m_value);
        }

        void reset() {
            if (await_ready()) {
                m_value.reset();
                m_except = nullptr;
                m_coro = nullptr;
            }
        }
    private:
        void gogogo() {
            if (m_coro) {
                auto liveness = m_liveness.lock();
                if (liveness) {
                    m_coro.resume();
                }
            }
        }
        std::optional<V> m_value;
        std::exception_ptr m_except;
        mutable std::coroutine_handle<> m_coro = std::noop_coroutine();
        mutable std::weak_ptr<bool> m_liveness = {};
        std::shared_ptr<bool> m_dummy = std::make_shared<bool>(true);
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
