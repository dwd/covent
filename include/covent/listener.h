//
// Created by dwd on 8/26/24.
//

#ifndef COVENT_LISTENER_H
#define COVENT_LISTENER_H

#include <memory>
#include <covent/core.h>
#include <covent/loop.h>

namespace covent {
    template<typename T>
    class Listener : public covent::ListenerBase {
    public:
        Listener(covent::Loop & l, unsigned short p) : covent::ListenerBase(l, p) {}
        void create_session(evutil_socket_t sock) override {
            loop().add(std::make_unique<T>(loop(), sock));
        }
    };
}

#endif //COVENT_LISTENER_H
