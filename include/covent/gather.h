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
}

#endif //COVENT_GATHER_H
