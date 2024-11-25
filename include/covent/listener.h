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
        Listener(covent::Loop & l, std::string const & address, unsigned short p) : covent::ListenerBase(l, address, p) {}
        void create_session(evutil_socket_t sock) override {
            loop().add(std::make_shared<T>(loop(), sock, *this));
        }
    };
}

#endif //COVENT_LISTENER_H
