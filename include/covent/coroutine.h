//
// Created by dwd on 8/24/24.
//

#ifndef COVENT_COROUTINE_H
#define COVENT_COROUTINE_H


#include "covent/base.h"
#include "exceptions.h"
#include "temp.h"
#include <coroutine>
#include <exception>
#include <stdexcept>
#include <optional>
#include <memory>
#include <sigslot/sigslot.h>
// #include <sentry.h>

namespace covent {
    // Usage: auto & my_promise = co_await own_promise<covent::task<bool>::promise_type>();
    template<typename P>
    struct own_promise {
        static constexpr bool no_loop_resume = true;
        mutable P * promise;
        bool await_ready() const noexcept {
            return false; // Say we'll suspend to get await_suspend called.
        }
        P & await_resume() const noexcept {
            return *promise;
        }
        bool await_suspend(std::coroutine_handle<P> h) const noexcept {
            promise = &h.promise();
            return false; // Never actually suspend.
        }
        auto & operator co_await() const {
            return *this;
        }
    };

    template<typename P>
    struct own_handle {
        static constexpr bool no_loop_resume = true;
        mutable std::coroutine_handle<P> handle;
        bool await_ready() const noexcept {
            return false; // Say we'll suspend to get await_suspend called.
        }
        std::coroutine_handle<P> await_resume() const noexcept {
            return handle;
        }
        bool await_suspend(std::coroutine_handle<P> h) const noexcept {
            handle = h;
            return false; // Never actually suspend.
        }
        auto & operator co_await() const {
            return *this;
        }
    };

    struct coroutine_switch {
        static constexpr bool no_loop_resume = true;
        std::coroutine_handle<> handle;
        [[nodiscard]] bool await_ready() const noexcept {
            return false; // Say we'll suspend to get await_suspend called.
        }
        void await_resume() const noexcept {}
        [[nodiscard]] std::coroutine_handle<> await_suspend(std::coroutine_handle<> h) const noexcept {
            return handle; // Always suspend, and switch to the other.
        }
        auto & operator co_await() const {
            return *this;
        }
    };

    namespace detail {
        class Stack {
            int i = 0;
//            sentry_transaction_t * trans = nullptr;
//            sentry_span_t * span = nullptr;
        };

        template<typename R>
        struct promise_base {
            static inline thread_local std::weak_ptr<Stack> thread_stack = {};
            std::exception_ptr eptr;
            std::coroutine_handle<> parent;
            std::shared_ptr<Stack> stack;
            sigslot::signal<> completed;
            /**
             * Double duty - a boolean that indicates if the coroutine has been started, and
             * also a shared_ptr to check (via weak_ptr) if the coroutine remains alive.
             * Only keep a weak_ptr ref to this!
             */
            std::shared_ptr<bool> liveness = std::make_shared<bool>(false);

            std::suspend_always initial_suspend() {
                this->stack = thread_stack.lock();
                if (!this->stack) {
                    stack = std::make_shared<Stack>();
                    restack();
                }
                return {};
            }

            void restack() const {
                thread_stack = this->stack;
            }

            struct final_awaiter
            {
                [[nodiscard]] bool await_ready() const noexcept { return false; }
                void await_resume() const noexcept {}
                template<typename P>
                std::coroutine_handle<> await_suspend(std::coroutine_handle<P> h) noexcept
                {
                    h.promise().completed.emit();
                    if (auto parent = h.promise().parent; parent) {
                        return parent;
                    }
                    return std::noop_coroutine();
                }
            };
            [[nodiscard]] final_awaiter final_suspend() const noexcept {
                return {};
            }

            void unhandled_exception() {
                eptr = std::current_exception();
            }

            void resolve() const {
                if (eptr) std::rethrow_exception(eptr);
            }
        };
        template<typename T> concept reference = std::is_reference_v<T>;
        template<typename T> concept refreference = std::is_rvalue_reference_v<T>;
        template<typename T> concept pointer = std::is_pointer_v<T>;
        template<typename T> concept copy = !std::is_trivial_v<T> && std::is_copy_constructible_v<T> && !std::is_move_constructible_v<T> && !std::is_pointer_v<T> && !std::is_reference_v<T> && !std::is_rvalue_reference_v<T>;
        template<typename T> concept move = !std::is_trivial_v<T> && std::is_move_constructible_v<T> && !std::is_pointer_v<T> && !std::is_reference_v<T> && !std::is_rvalue_reference_v<T>;
        template<typename T> concept trivial = std::is_trivial_v<T> && !std::is_pointer_v<T> && !std::is_reference_v<T> && !std::is_rvalue_reference_v<T>;

        template<typename R, typename V=typename R::value_type> struct promise;

        template<typename R, reference V>
        struct promise<R,V> : promise_base<R> {
            using value_type = std::remove_reference_t<V>;
            using handle_type = std::coroutine_handle<promise<R, V>>;
            value_type * value;

            R get_return_object() {
                return R{handle_type::from_promise(*this)};
            }

            std::suspend_never return_value(value_type & v) {
                value = &v;
                return {};
            }

            value_type & get() {
                this->resolve();
                return *value;
            }
        };

        template<typename R, pointer V>
        struct promise<R,V> : promise_base<R> {
            using value_type = std::remove_pointer_t<V>;
            using handle_type = std::coroutine_handle<promise<R, V>>;
            value_type * value;

            R get_return_object() {
                return R{handle_type::from_promise(*this)};
            }

            std::suspend_never return_value(value_type * v) {
                value = v;
                return {};
            }

            value_type * get() {
                this->resolve();
                return value;
            }
        };

        template<typename R, refreference V>
        struct promise<R,V> : promise_base<R> {
            using value_type = std::remove_reference_t<V>;
            using handle_type = std::coroutine_handle<promise<R, V>>;
            std::optional<value_type> value;

            R get_return_object() {
                return R{handle_type::from_promise(*this)};
            }

            std::suspend_never return_value(value_type && v) {
                value.emplace(std::move(v));
                return {};
            }

            value_type get() {
                this->resolve();
                return std::move(value.value());
            }
        };

        template<typename R, move V>
        struct promise<R,V> : promise_base<R> {
            using value_type = V;
            using handle_type = std::coroutine_handle<promise<R, V>>;
            std::optional<value_type> value;

            R get_return_object() {
                return R{handle_type::from_promise(*this)};
            }

            std::suspend_never return_value(value_type && v) {
                value.emplace(std::move(v));
                return {};
            }

            value_type get() {
                this->resolve();
                return std::move(value.value());
            }
        };

        template<typename R, copy V>
        struct promise<R,V> : promise_base<R> {
            using value_type = V;
            using handle_type = std::coroutine_handle<promise<R, V>>;
            std::optional<value_type> value;

            R get_return_object() {
                return R{handle_type::from_promise(*this)};
            }

            std::suspend_never return_value(value_type const & v) {
                value.emplace(v);
                return {};
            }

            value_type const & get() {
                this->resolve();
                return value.value();
            }
        };

        template<typename R, trivial V>
        struct promise<R,V> : promise_base<R> {
            using value_type = V;
            using handle_type = std::coroutine_handle<promise<R, V>>;
            std::optional<value_type> value;

            R get_return_object() {
                return R{handle_type::from_promise(*this)};
            }

            std::suspend_never return_value(value_type v) {
                value.emplace(v);
                return {};
            }

            value_type get() {
                this->resolve();
                return value.value();
            }
        };

        template<typename R>
        struct promise<R, void> : promise_base<R> {
            using handle_type = std::coroutine_handle<promise<R, void>>;

            R get_return_object() {
                return R{handle_type::from_promise(*this)};
            }

            std::suspend_never return_void() const {
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
                if (task.handle.done()) {
                    return coro;
                }
                if (*task.handle.promise().liveness) {
                    // Already started, but presumably suspended, so do nothing.
                    return std::noop_coroutine();
                }
                *task.handle.promise().liveness = true;
                return task.handle;
            }
        };
    }

    template<typename R>
    struct [[nodiscard]] instant_task {
        using value_type = R;
        using promise_type = detail::promise<instant_task<value_type>>;
        using handle_type = std::coroutine_handle<promise_type>;

        handle_type handle = nullptr;

        bool start() const { // NOLINT
            *handle.promise().liveness = true;
            handle.resume();
            return handle.done();
        }
        [[nodiscard]] bool done() const {
            return handle.done();
        }
        value_type get() const {
            if (!done()) {
                throw covent_logic_error("coroutine not done yet");
            }
            return handle.promise().get();
        }
        template<typename... Args>
        auto on_completed(Args... a) {
            return this->handle.promise().completed.connect(a...);
        }

        instant_task() = default;
        explicit instant_task(handle_type h) : handle(h) {}
        instant_task(instant_task && other) noexcept : handle(other.handle) {
            other.handle = nullptr;
        }
        auto & operator = (instant_task && other) noexcept {
            handle = other.handle;
            other.handle = nullptr;
            return *this;
        }
        ~instant_task() { if (handle) handle.destroy(); }

        auto operator co_await() const {
            return detail::task_awaiter(*this);
        }
    };

    namespace detail {

        template<typename A, typename P, typename AT = decltype(std::declval<A>().operator co_await()), typename VT = decltype(std::declval<AT>().await_resume())>
        struct await_transformer_base {
            using awaiter_type = AT;
            using value_type = VT;
            covent::temp<value_type> ret;
            instant_task<void> runner;
            covent::temp<A &> real;

            explicit await_transformer_base(covent::temp<A&> && a) : real(std::move(a)) {}

            void resume(std::coroutine_handle<P> coro, covent::instant_task<void>::promise_type & p) {
                // TODO: Shuffle virtual callstack, schedule call.
                if (coro.promise().same_loop()) {
                    p.parent = coro;
                    return;
                }
                auto * l = coro.promise().loop;
                std::weak_ptr<bool> liveness = coro.promise().liveness;

                l->defer([coro, liveness](){
                    if (auto alive = liveness.lock(); alive) {
                        coro.promise().restack();
                        coro.resume();
                    }
                });
            }

            auto await_ready() const {
                // We always return false here to force our mini coroutine to run.
                // It'd be nice to avoid this,
                return false;
            }
            VT await_resume() {
                runner.get();
                return ret.value();
            }
        };

        template<typename A, typename P, typename AT = decltype(std::declval<A>().operator co_await()), typename VT = decltype(std::declval<AT>().await_resume())>
        struct await_transformer : public await_transformer_base<A, P, AT, VT> {
            using await_transformer_base<A,P,AT,VT>::await_transformer_base;
            instant_task<void> wrapper(std::coroutine_handle<P> coro) {
                // TODO : No longer running, should mark suspended.
                auto & p = co_await own_promise<covent::instant_task<void>::promise_type>();
                try {
                    this->ret.assign(co_await this->real.value());
                } catch (...) {
                    p.unhandled_exception();
                }
                this->resume(coro, p);
            }
            auto await_suspend(std::coroutine_handle<P> coro) {
                this->runner = wrapper(coro);
                return this->runner.handle;
            }
        };

        template<typename A, typename P, typename AT>
        struct await_transformer<A, P, AT, void> : public await_transformer_base<A, P , AT, void> {
            using await_transformer_base<A,P,AT,void>::await_transformer_base;
            instant_task<void> wrapper(std::coroutine_handle<P> coro) {
                // TODO : No longer running, should mark suspended.
                auto & p = co_await own_promise<covent::instant_task<void>::promise_type>();
                try {
                    co_await this->real.value();
                    this->ret.assign();
                } catch (...) {
                    p.unhandled_exception();
                }
                this->resume(coro, p);
            }
            auto await_suspend(std::coroutine_handle<P> coro) {
                this->runner = wrapper(coro);
                return this->runner.handle;
            }
        };

        // Force the loop into a template parameter to side-step needing to declare it fully.
        template<typename R, typename L=Loop>
        struct wrapped_promise : promise<R> {
            L * loop;

            wrapped_promise() : loop(&L::thread_loop()) {}
            template<typename ...Args>
            wrapped_promise(L & l, Args & ...other_args) : loop(&l) {}

            bool same_loop() {
                return loop == &L::thread_loop();
            }

            using handle_type = std::__n4861::coroutine_handle<wrapped_promise<R>>;
            R get_return_object() {
                return R{handle_type::from_promise(*this)};
            }

            template<typename A>
            requires A::no_loop_resume
            auto const & await_transform(const A & a) const {
                return a;
            }

            template<typename A>
            auto await_transform(A & a) const {
                covent::temp<A &> aa;
                aa.assign(a);
                return await_transformer<A, wrapped_promise<R>>{std::move(aa)};
            }
            template<typename A>
            auto await_transform(const A & a) const {
                covent::temp<const A &> aa;
                aa.assign(a);
                return await_transformer<const A, wrapped_promise<R>>{std::move(aa)};
            }
        };
    }

    template<typename R, typename L>
    struct [[nodiscard]] task {
        using value_type = R;
        using promise_type = detail::wrapped_promise<task<value_type>>;
        using handle_type = std::coroutine_handle<promise_type>;

        handle_type handle;
        bool start() const { // NOLINT
            if (!handle) throw std::logic_error("No such coroutine");
            if (handle.promise().same_loop()) {
                *handle.promise().liveness = true;
                handle.resume();
            } else {
                auto * l = handle.promise().loop;
                std::weak_ptr<bool> alive = handle.promise().liveness;

                l->defer([handle= this->handle, alive](){
                    if (auto liveness = alive.lock(); liveness) {
                        *handle.promise().liveness = true;
                        handle.resume();
                    }
                });
            }
            return handle.done();
        }
        [[nodiscard]] bool done() const {
            if (!handle) throw std::logic_error("No such coroutine");
            return handle.done();
        }
        value_type get() const {
            if (!done()) {
                throw covent_logic_error("coroutine not done yet");
            }
            return handle.promise().get();
        }

        task() : handle(nullptr) {}
        explicit task(handle_type h) : handle(h) {}
        task(task && other) noexcept : handle(other.handle) {
            other.handle = nullptr;
        }
        ~task() {
            if (handle) handle.destroy();
        }
        task & operator = (task && other) noexcept {
            if(handle) handle.destroy();
            handle = other.handle;
            other.handle = nullptr;
            return *this;
        }
        auto operator co_await() const {
            return detail::task_awaiter(*this);
        }
        template<typename... Args>
        auto on_completed(Args... a) {
           return this->handle.promise().completed.connect(a...);
        }
    };
}

#endif //COVENT_COROUTINE_H
