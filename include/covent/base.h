//
// Created by dwd on 8/24/24.
//

#ifndef COVENT_BASE_H
#define COVENT_BASE_H

// Forward declarations and constants

namespace covent {
    class Loop;
    class Session;
    template<typename T, typename L=Loop>
    struct task;
    template<typename T>
    struct instant_task;
    class Service;
    namespace dns {
        class Resolver;
    }
    namespace pkix {
        class TLSContext;
    }
}

#endif //COVENT_BASE_H
