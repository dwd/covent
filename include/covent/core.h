//
// Created by dwd on 8/26/24.
//

#ifndef COVENT_CORE_H
#define COVENT_CORE_H

#include <event2/util.h>
#include <set>
#include <mutex>
#include <optional>
#include <functional>
#include <coroutine>
#include <map>
#include <memory>
#include <vector>
#include <sys/socket.h>
#include <covent/coroutine.h>
#include <covent/base.h>
#include <covent/sockaddr-cast.h>
#include <sigslot/sigslot.h>
#include <openssl/types.h>
#include "future.h"

struct bufferevent;

namespace covent {
    class ListenerBase {
    public:
        ListenerBase(Loop & loop, std::string const & address, unsigned short port);

        const struct sockaddr * sockaddr() const;
        void session_connected(evutil_socket_t sock, const struct sockaddr * addr, int len);

        virtual void create_session(evutil_socket_t) = 0;
        Loop & loop() {
            return m_loop;
        }
        virtual ~ListenerBase() = default;

    private:
        Loop & m_loop;
        unsigned short m_port;
        struct sockaddr_storage m_sockaddr;
    };

    class Session : public sigslot::has_slots {
    public:
        explicit Session(Loop & loop);
        Session(Loop & loop, evutil_socket_t sock, ListenerBase &);
        Session() = delete;
        Session(Session const &) = delete;
        Session(Session &&) = delete;

        virtual ~Session();

        using id_type = uint64_t;

        // Consume whatever data you can from the buffer. If that's actually none of it, immediately return 0.
        // Anything you do consume, return the number of octets you used (this will destroy buffers).
        virtual task<std::size_t> process(std::string_view data) = 0;

        virtual void closed() {}

        void write(std::string_view data); // Fire and forget writing.
        [[nodiscard]] task<void> flush(std::string_view data = {}); // Awaitable writing.
        SSL * ssl() const;
        void ssl(SSL * s, bool connecting);

        task<void> connect(const struct sockaddr *, size_t);
        template<typename S>
        auto connect(S * addr) {
            auto * base_addr = sockaddr_cast<AF_UNSPEC>(addr);
            return connect(base_addr, sizeof(S));
        }
        void close();

        id_type id() const {
            return m_id;
        }

        Loop & loop() const {
            return m_loop;
        }

        void used(size_t len);
        void read_cb(struct bufferevent * bev);
        void write_cb(struct bufferevent * bev);
        void event_cb(struct bufferevent * bev, short flags);
        void processing_complete();
    private:
        id_type m_id;
        Loop & m_loop;
        // this will always be a socket bufferevent.
        struct bufferevent * m_base_buf = nullptr;
        // This might be the same as above, but more likely a TLS (or other filter).
        struct bufferevent * m_top = nullptr;
        std::optional<task<std::size_t>> m_processor;
        future<void> connected;
        future<void> written;
    };
}


#endif //COVENT_CORE_H
