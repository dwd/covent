//
// Created by dwd on 1/22/25.
//

#ifndef GENERATOR_H
#define GENERATOR_H

#include <coroutine>
#include <iterator>

#include "coroutine.h"

namespace covent {
    template<typename T>
    class [[nodiscard]] generator {
    public:
        using value_pointer = std::remove_reference_t<T> *;
        struct handle_type;
        struct promise_type {
            value_pointer value;

            std::suspend_always yield_value(T & v) {
                value = &v;
                return {};
            }

            static std::suspend_never initial_suspend() {
                return {};
            }

            static std::suspend_always final_suspend() noexcept {
                return {}; // Change this to std::suspend_always
            }

            static void return_void() {}

            [[noreturn]] static void unhandled_exception() {
                std::rethrow_exception(std::current_exception());
            }

            generator get_return_object() {
                return generator{handle_type{handle_type::from_promise(*this)}};
            }
        };

        struct handle_type : std::coroutine_handle<promise_type> {
            explicit handle_type(std::coroutine_handle<promise_type> && h) : std::coroutine_handle<promise_type>(std::move(h)) {}

            T &operator*() {
                return *(this->promise().value);
            }

            void operator++() {
                this->resume();
            }

            bool operator!=(std::default_sentinel_t) const {
                return !this->done();
            }
        };

        explicit generator(handle_type h) : m_handle(h) {}

        ~generator() {
            if (m_handle)
                m_handle.destroy();
        }

        handle_type begin() {
            return m_handle;
        }

        static std::default_sentinel_t end() {
            return std::default_sentinel;
        }

    private:
        [[no_unique_address]] handle_type m_handle;
    };

    template<typename T>
    class [[nodiscard]] generator_async {
    public:
        using value_pointer = std::remove_reference_t<T> *;
        struct handle_type;
        struct promise_type {
            covent::future<T> value = {};
            std::coroutine_handle<> consumer;

            struct resumer {
                std::coroutine_handle<> consumer;

                static bool await_ready() noexcept {
                    return false;
                }
                static void await_resume() noexcept {}
                [[nodiscard]] std::coroutine_handle<> await_suspend(std::coroutine_handle<>) const noexcept {
                    if (consumer) {
                        return consumer;
                    }
                    return std::noop_coroutine();
                }
            };

            resumer yield_value(T & v) {
                value.resolve(v);
                return {consumer};
            }

            std::suspend_always initial_suspend() {
                return {};
            }

            resumer final_suspend() noexcept {
                return {consumer}; // Change this to std::suspend_always
            }

            void return_void() {}

            void unhandled_exception() {
                std::terminate();
            }

            generator_async get_return_object() {
                return generator_async{handle_type{handle_type::from_promise(*this)}};
            }
        };

        struct handle_type : std::coroutine_handle<promise_type> {
            explicit handle_type(std::coroutine_handle<promise_type> && h) : std::coroutine_handle<promise_type>(std::move(h)) {}

            T operator*() {
                return this->promise().value.await_resume();
            }

            covent::task<void> operator++() {
                this->promise().value.reset();
                auto p = co_await own_handle<task<void>::promise_type>{};
                this->promise().consumer = p;
                co_await coroutine_switch(*this);
            }

            bool operator!=(std::default_sentinel_t) const {
                return !this->done();
            }
        };

        explicit generator_async(handle_type h) : m_handle(h) {}

        ~generator_async() {
            if (m_handle)
                m_handle.destroy();
        }

        covent::task<handle_type &> begin() {
            auto p = co_await own_handle<typename task<handle_type &>::promise_type>{};
            m_handle.promise().consumer = p;
            co_await coroutine_switch(m_handle);
            co_return m_handle;
        }

        std::default_sentinel_t end() {
            return std::default_sentinel;
        }

    private:
        [[no_unique_address]] handle_type m_handle;
    };
}

#endif //GENERATOR_H
