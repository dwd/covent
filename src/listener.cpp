//
// Created by dwd on 8/26/24.
//

#include <covent/core.h>
#include <covent/loop.h>
#include <event2/listener.h>

covent::ListenerBase::ListenerBase(covent::Loop &loop, std::string const & address, unsigned short port) :m_loop(loop), m_port(port), m_sockaddr() {
    std::memset(&m_sockaddr, 0, sizeof(m_sockaddr)); // Clear, to avoid valgrind complaints later.
    if (1 == inet_pton(AF_INET6, address.c_str(), &(covent::sockaddr_cast<AF_INET6>(&m_sockaddr)->sin6_addr))) {
        auto *sa = covent::sockaddr_cast<AF_INET6>(&m_sockaddr);
        sa->sin6_family = AF_INET6;
        sa->sin6_port = htons(port);
    } else if (1 == inet_pton(AF_INET, address.c_str(), &(covent::sockaddr_cast<AF_INET>(&m_sockaddr)->sin_addr))) {
        auto *sa = covent::sockaddr_cast<AF_INET>(&m_sockaddr);
        sa->sin_family = AF_INET;
        sa->sin_port = htons(port);
    } else {
        throw std::runtime_error("Couldn't understand address syntax " + std::string(address));
    }
}

const struct sockaddr *covent::ListenerBase::sockaddr() const {
    return sockaddr_cast<AF_UNSPEC>(&m_sockaddr);
}

void covent::ListenerBase::session_connected(int sock, struct sockaddr const *, int) {
    create_session(sock);
}

namespace {
    void listener_connect_cb(struct evconnlistener *, evutil_socket_t sock, struct sockaddr * addr, int len, void * arg) {
        auto listener = static_cast<covent::ListenerBase *>(arg);
        listener->session_connected(sock, addr, len);
    }
}

void covent::ListenerBase::listen(covent::Loop & loop) {
    m_listener = evconnlistener_new_bind(loop.event_base(), listener_connect_cb, this, LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE, -1, sockaddr(), sizeof(struct sockaddr_storage));
}

covent::ListenerBase::~ListenerBase() {
    if (m_listener) evconnlistener_free(m_listener);
}