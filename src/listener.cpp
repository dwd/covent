//
// Created by dwd on 8/26/24.
//

#include <covent/core.h>

covent::ListenerBase::ListenerBase(covent::Loop &loop, unsigned short port) :m_loop(loop), m_port(port), m_addr() {
    auto * addr = sockaddr_cast<AF_INET6>(&m_addr);
    addr->sin6_family = AF_INET6;
    addr->sin6_addr = IN6ADDR_ANY_INIT;
    addr->sin6_port = htons(m_port);
}

const struct sockaddr *covent::ListenerBase::sockaddr() const {
    return sockaddr_cast<AF_UNSPEC>(&m_addr);
}

void covent::ListenerBase::session_connected(int sock, struct sockaddr const *, int) {
    create_session(sock);
}
