//
// Created by dwd on 11/14/24.
//

#ifndef COVENT_GATHER_H
#define COVENT_GATHER_H

#include <tuple>

#include <covent/coroutine.h>

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
    }

    // Usage:
    // R foo = race({task(), task2(), ...});
    template<typename R>
    covent::task<R> race(std::initializer_list<covent::task<R>> tasks) {
        // Kick off by starting each task. If any finishes instantly, just return the result.
        for (auto & task : tasks) {
            if (task.start()) {
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
        // Now co_wait the dummy, making us the (grand)parent of the tasks:
        co_await dummy_task;
        // So, one of the tasks has finished - look through and find it, and return the result.
        for (auto & task : tasks) {
            if (task.done()) {
                co_return task.get();
            }
        }
        throw std::logic_error("Race was not won by anyone?");
    }
}

#endif //COVENT_GATHER_H
