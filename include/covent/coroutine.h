//
// Created by dwd on 8/24/24.
//

#ifndef COVENT_COROUTINE_H
#define COVENT_COROUTINE_H


#include "covent/base.h"
#include "exceptions.h"
#include <coroutine>
#include <exception>
#include <stdexcept>
#include <optional>

namespace covent {
    namespace detail {

        template<typename R>
        struct promise_base {
            std::exception_ptr eptr;
            std::coroutine_handle<> parent;

            std::suspend_always initial_suspend() const {
                return {};
            }

            std::suspend_always final_suspend() const noexcept {
                return {};
            }

            void unhandled_exception() {
                eptr = std::current_exception();
            }

            void resolve() {
                if (eptr) std::rethrow_exception(eptr);
            }
        };

        template<typename R, typename V=R::value_type>
        struct promise : promise_base<R> {
            V value;
            using handle_type = std::__n4861::coroutine_handle<promise<R, V>>;

            R get_return_object() {
                return R{handle_type::from_promise(*this)};
            }

            std::suspend_never return_value(V v) {
                value = v;
                return {};
            }

            auto get() {
                this->resolve();
                return value;
            }
        };

        template<typename R>
        struct promise<R, void> : promise_base<R> {
            using handle_type = std::__n4861::coroutine_handle<promise<R, void>>;

            R get_return_object() {
                return R{handle_type::from_promise(*this)};
            }

            std::suspend_never return_void() {
                return {};
            }

            void get() {
                this->resolve();
            }
        };

        template<typename T>
        struct task_awaiter {
            T const &task;

            bool await_ready() {
                return task.handle.done();
            }

            auto await_resume() {
                return task.handle.promise().get();
            }

            std::coroutine_handle<> await_suspend(std::coroutine_handle<> coro) {
                // coro is the currently executing coroutine on this thread.
                // We're going to start our new one, but first we'll let it know who its parent is and let it set up its execution context:
                task.handle.promise().parent = coro;
                task.start();
                if (task.handle.done()) {
                    return coro;
                }
                return task.handle;
            }
        };
    }

    template<typename R>
    struct instant_task {
        using value_type = R;
        using promise_type = detail::promise<instant_task<value_type>>;
        using handle_type = std::coroutine_handle<promise_type>;

        handle_type handle;

        bool start() const {
            handle.resume();
            return handle.done();
        }
        bool done() const {
            return handle.done();
        }
        value_type get() const {
            if (!done()) throw covent_logic_error("coroutine not done yet");
            return handle.promise().get();
        }

        explicit instant_task(handle_type h) : handle(h) {}
        instant_task(instant_task && other) noexcept : handle(other.handle) {
            other.handle = nullptr;
        }
        ~instant_task() { if (handle) handle.destroy(); }

        auto operator co_await() const {
            return detail::task_awaiter(*this);
        }
    };

    namespace detail {

        template<typename A, typename P, typename AT = decltype(std::declval<A>().operator co_await()), typename VT = decltype(std::declval<AT>().await_resume())>
        struct await_transformer {
            using awaiter_type = AT;
            using value_type = VT;
            std::optional<value_type> ret;
            std::optional<instant_task<void>> runner;
            A const & real;

            explicit await_transformer(A const & a) : real(a) {}

            instant_task<void> wrapper(std::coroutine_handle<P> coro) {
                // TODO : No longer running, should mark suspended.
                ret = co_await real;
                // TODO: Shuffle virtual callstack, schedule call.
                auto * l = coro.promise().loop;
                l->defer([coro](){ coro.resume(); });
            }

            auto await_ready() {
                // We always return false here to force our mini coroutine to run.
                // It'd be nice to avoid this,
                return false;
            }
            auto await_resume() {
                return ret.value();
            }
            auto await_suspend(std::coroutine_handle<P> coro) {
                runner.emplace(wrapper(coro));
                return runner.value().handle;
            }
        };

        template<typename A, typename P, typename AT>
        struct await_transformer<A, P, AT, void> {
            using awaiter_type = AT;
            using value_type = void;
            std::optional<instant_task<void>> runner;
            A const & real;

            explicit await_transformer(A const & a) : real(a) {}

            instant_task<void> wrapper(std::coroutine_handle<P> coro) {
                // TODO : No longer running, should mark suspended.
                co_await real;
                // TODO: Shuffle virtual callstack, schedule call.
                auto * l = coro.promise().loop;
                l->defer([coro](){ coro.resume(); });
            }

            auto await_ready() {
                // We always return false here to force our mini coroutine to run.
                // It'd be nice to avoid this,
                return false;
            }
            auto await_resume() {
                return;
            }
            auto await_suspend(std::coroutine_handle<P> coro) {
                runner.emplace(wrapper(coro));
                return runner.value().handle;
            }
        };

        // Force the loop into a template parameter to side-step needing to declare it fully.
        template<typename R, typename L=Loop>
        struct wrapped_promise : promise<R> {
            L * loop;

            wrapped_promise() : loop(&L::thread_loop()) {}
            explicit wrapped_promise(L & l) : loop(&l) {}

            using handle_type = std::__n4861::coroutine_handle<wrapped_promise<R>>;
            R get_return_object() {
                return R{handle_type::from_promise(*this)};
            }

            template<typename A>
            auto await_transform(A const & a) {
                return await_transformer<A, wrapped_promise<R>>{a};
            }
        };
    }

    template<typename R, typename L>
    struct task {
        using value_type = R;
        using promise_type = detail::wrapped_promise<task<value_type>>;
        using handle_type = std::coroutine_handle<promise_type>;

        handle_type handle;
        bool start() const {
            if (handle.promise().loop == &L::thread_loop()) {
                handle.resume();
            } else {
                auto * l = handle.promise().loop;
                l->defer([this](){ handle.resume(); });
            }
            return handle.done();
        }
        bool done() const {
            return handle.done();
        }
        value_type get() const {
            if (!done()) throw covent_logic_error("coroutine not done yet");
            return handle.promise().get();
        }

        explicit task(handle_type h) : handle(h) {}
        task(task && other) noexcept : handle(other.handle) {
            other.handle = nullptr;
        }
        ~task() { if (handle) handle.destroy(); }
        auto operator co_await() const {
            return detail::task_awaiter(*this);
        }
    };
}

#endif //COVENT_COROUTINE_H
