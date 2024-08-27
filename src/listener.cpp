//
// Created by dwd on 8/26/24.
//

#include <covent/core.h>

covent::ListenerBase::ListenerBase(covent::Loop &loop, unsigned short port) : m_loop(loop), m_port(port) {
    auto * addr = reinterpret_cast<struct sockaddr_in6 *>(&m_addr);
    addr->sin6_family = AF_INET6;
    addr->sin6_addr = IN6ADDR_ANY_INIT;
    addr->sin6_port = htons(m_port);
}

const struct sockaddr *covent::ListenerBase::sockaddr() const {
    return reinterpret_cast<const struct sockaddr *>(&m_addr);
}

void covent::ListenerBase::session_connected(int sock, struct sockaddr *addr, int len) {
    create_session();
}