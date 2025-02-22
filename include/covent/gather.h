//
// Created by dwd on 11/14/24.
//

#ifndef COVENT_GATHER_H
#define COVENT_GATHER_H

#include <tuple>

#include <covent/coroutine.h>

#include "sleep.h"

namespace covent {

    template<typename R>
    concept task_type = std::same_as<typename std::remove_reference_t<R>, covent::task<typename std::remove_reference_t<R>::value_type>>;

    template<typename C>
    using element_type = std::decay_t<decltype(*std::begin(std::declval<C>()))>;
    template <typename Container>
    concept task_container = requires(Container const & c) {
        { c.cbegin() } -> std::input_iterator;
        { c.cend() } -> std::input_iterator;
        requires task_type<element_type<Container const &>>;
    };

    // Usage:
    // auto [ ... ] = co_await gather(task(), task2(), ...);
    template<typename ...Args>
    covent::task<std::tuple<Args...>> gather(covent::task<Args>... tasks) {
        (tasks.start(), ...);
        co_return std::make_tuple(co_await tasks...);
    }

    template<task_container C, typename R=typename element_type<C>::value_type>
    covent::task<std::list<R>> gather(C const & container) {
        for (const auto & task : container) task.start();
        std::list<R> result;
        for (const auto & task : container) result.emplace_back(co_await task);
        co_return result;
    }

    namespace detail {
        struct forever {};
    }

    template<typename T>
    concept timer_value = std::same_as<T, detail::forever> || std::same_as<T, struct timeval> || std::convertible_to<T, unsigned long> || std::convertible_to<T, double>;

    namespace detail {
        covent::task<void> dummy();
        template<typename T>
        covent::task<void> timeout(T t) {
            co_await covent::sleep(t);
        }
    }
    class race_timeout : public std::runtime_error {
    public:
        using std::runtime_error::runtime_error;
    };

    // Usage:
    // R foo = race({task(), task2(), ...});
    template<task_container C, timer_value T, typename R=C::value_type::value_type>
    covent::task<R> race_container(C const & tasks, T timeout) {
        // Kick off by starting each task. If any finishes instantly, just return the result.
        covent::task<void> timer;
        for (auto & task : tasks) {
            if (!*task.handle.promise().liveness && task.start()) {
                co_return task.get();
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
        // Detach it, otherwise things will explode should any of the other tasks complete later.
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

    namespace detail {
        template<task_type Task>
        Task *return_first_done(Task & first) {
            if (first.done()) {
                return &first;
            }
            return nullptr;
        }

        template<task_type Task, task_type ...Tasks>
        Task *return_first_done(Task & first, Tasks &... rest) {
            if (first.done()) {
                return &first;
            }
            return return_first_done(rest...);
        }
    }

    template<typename R, task_type ...Tasks, timer_value T>
    covent::task<R> race_pack(T timeout, Tasks &... tasks) {
        // Kick off by starting each task. If any finishes instantly, just return the result.
        covent::task<void> timer;
        ((!*tasks.handle.promise().liveness && tasks.start()), ...);
        {
            auto *winner = detail::return_first_done(tasks...);
            if (winner) co_return winner->get();
        }
        // We now have a bunch of tasks which are suspended, so we only want to await the first.
        auto dummy_task = detail::dummy();
        // Fake it starting.
        *dummy_task.handle.promise().liveness = true;
        // OK, that was weird. Now make it the parent of all the tasks passed in:
        ((tasks.handle.promise().parent = dummy_task.handle), ...);
        if constexpr (!std::is_same_v<T, detail::forever>) {
            timer = detail::timeout(timeout);
            timer.handle.promise().parent = dummy_task.handle;
            timer.start();
        }
        // Now co_wait the dummy, making us the (grand)parent of the tasks:
        co_await dummy_task;
        // Detach it, otherwise things will explode should any of the other tasks complete later.
        ((tasks.handle.promise().parent = std::coroutine_handle<>{}), ...);
        // So, one of the tasks has finished - look through and find it, and return the result.
        {
            auto *winner = detail::return_first_done(tasks...);
            if (winner) co_return winner->get();
        }
        throw race_timeout("Race timeout");
    }

    template<task_type Task, task_type ...Tasks, timer_value T>
    auto race(Task task, Tasks... tasks, T timeout) {
        return race_pack<typename Task::value_type>(timeout, task, tasks...);
    }
    template<task_type Task, timer_value T>
    auto race(Task task, T timeout) {
        return race_pack<typename Task::value_type>(timeout, task);
    }
    template<task_type Task, task_type ...Tasks>
    auto race(Task task, Tasks... tasks) {
        return race_pack<typename Task::value_type>(detail::forever{}, task, tasks...);
    }
    template<task_type Task>
    auto race(Task task) {
        return race_pack<typename Task::value_type>(detail::forever{},task);
    }
    template<task_container Task, timer_value T>
    auto race(Task const & task, T timeout) {
        return race_container(task, timeout);
    }
    template<task_container Task>
    auto race(Task const & task) {
        return race_container(task, detail::forever{});
    }
}

#endif //COVENT_GATHER_H
