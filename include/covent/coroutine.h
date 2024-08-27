//
// Created by dwd on 8/24/24.
//

#ifndef COVENT_COROUTINE_H
#define COVENT_COROUTINE_H


#include "covent/base.h"
#include <coroutine>
#include <exception>
#include <optional>

namespace covent {
    namespace detail {

        template<typename R>
        struct promise_base {
            std::exception_ptr eptr;
            std::coroutine_handle<> parent;

            std::suspend_always initial_suspend() {
                return {};
            }

            std::suspend_always final_suspend() noexcept {
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
                return handle_type::from_promise(*this);
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
                return handle_type::from_promise(*this);
            }

            std::suspend_never return_void() {
                return {};
            }
        };

        template<typename T>
        struct task_awaiter {
            T &task;

            bool await_ready() {
                return task.handle.done();
            }

            auto await_resume() {
                return task.handle.promise().get();
            }

            auto await_suspend(std::coroutine_handle<> coro) {
                // coro is the currently executing coroutine on this thread.
                // We're going to start our new one, but first we'll let it know who its parent is and let it set up its execution context:
                task.handle.promise().parent = coro;
                task.start();
                if (task.handle.finished()) {
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

        explicit instant_task(handle_type h) : handle(h) {}
        ~instant_task() { handle.destroy(); }
    };

    namespace detail {

        template<typename A>
        struct await_transformer {
            using value_type = decltype(A{}.await_resume());
            std::optional<value_type> ret;
            std::optional<instant_task<void>> runner;
            A real;

            explicit await_transformer(A && a) : real(a) {}

            instant_task<void> wrapper(std::coroutine_handle<> coro) {
                // TODO : No longer running, should mark suspended.
                ret = co_await real;
                // TODO: Shuffle virtual callstack, schedule call.
                coro.resume();
            }

            auto await_ready() {
                return real.await_ready();
            }
            auto await_resume() {
                return ret;
            }
            auto await_suspend(std::coroutine_handle<> coro) {
                if (real.await_ready()) return coro;
                runner = wrapper(coro);
                return runner.value().handle;
            }
        };

        template<typename R>
        struct wrapped_promise : promise<R> {
            using handle_type = std::__n4861::coroutine_handle<wrapped_promise<R>>;
            R get_return_object() {
                return R{handle_type::from_promise(*this)};
            }

            template<typename A>
            auto await_transform(A && a) {
                return await_transformer{a};
            }
        };
    }

    template<typename R>
    struct task {
        using value_type = R;
        using promise_type = detail::wrapped_promise<task<value_type>>;
        using handle_type = std::coroutine_handle<promise_type>;

        handle_type handle;

        explicit task(handle_type h) : handle(h) {}
        ~task() { handle.destroy(); }
    };
}

#endif //COVENT_COROUTINE_H
