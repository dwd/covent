//
// Created by dwd on 11/14/24.
//

#ifndef COVENT_GATHER_H
#define COVENT_GATHER_H

#include <tuple>

#include <covent/coroutine.h>

#include "sleep.h"

namespace covent {
    // Usage:
    // auto [ ... ] = co_await gather(task(), task2(), ...);
    template<typename ...Args>
    covent::task<std::tuple<Args...>> gather(covent::task<Args>... tasks) {
        (tasks.start(), ...);
        co_return std::make_tuple(co_await tasks...);
    }

    namespace detail {
        covent::task<void> dummy();
        template<typename T>
        covent::task<void> timeout(T t) {
            co_await covent::sleep(t);
        }
        struct forever {};
    }
    class race_timeout : public std::runtime_error {
    public:
        using std::runtime_error::runtime_error;
    };
    template <typename Container, typename T>
    concept container_of = requires(Container c) {
        typename Container::value_type;
        requires std::same_as<typename Container::value_type, T>;
        { c.begin() } -> std::input_iterator;
        { c.end() } -> std::input_iterator;
    };

    // Usage:
    // R foo = race({task(), task2(), ...});
    template<typename R, container_of<covent::task<R>> C, typename T=detail::forever>
    covent::task<R> race(C const & tasks, T timeout={}, bool start=true) {
        // Kick off by starting each task. If any finishes instantly, just return the result.
        covent::task<void> timer;
        if (start) {
            for (auto & task : tasks) {
                if (task.start()) {
                    co_return task.get();
                }
            }
        }
        // We now have a bunch of tasks which are suspended, so we only want to await the first.
        auto dummy_task = detail::dummy();
        // Fake it starting.
        *dummy_task.handle.promise().liveness = true;
        // OK, that was weird. Now make it the parent of all the tasks passed in:
        for (auto & task : tasks) {
            task.handle.promise().parent = dummy_task.handle;
        }
        if constexpr (!std::is_same_v<T, detail::forever>) {
            timer = detail::timeout(timeout);
            timer.handle.promise().parent = dummy_task.handle;
            timer.start();
        }
        // Now co_wait the dummy, making us the (grand)parent of the tasks:
        co_await dummy_task;
        // Detatch it, otherwise things will explode should any of the other tasks complete later.
        for (auto & task : tasks) {
            task.handle.promise().parent = std::coroutine_handle<>{};
        }
        // So, one of the tasks has finished - look through and find it, and return the result.
        for (auto & task : tasks) {
            if (task.done()) {
                co_return task.get();
            }
        }
        throw race_timeout("Race timeout");
    }
    template<typename R>
    covent::task<R> race(std::initializer_list<covent::task<R>> const & tasks) {
        return race<R>(tasks, detail::forever{}, true);
    }
}

#endif //COVENT_GATHER_H
