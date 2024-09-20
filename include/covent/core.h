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
#include <sigslot/sigslot.h>

struct bufferevent;

namespace covent {
    class Session {
    public:
        Session(Loop & loop);
        Session(Loop & loop, evutil_socket_t sock);
        Session() = delete;
        Session(Session const &) = delete;
        Session(Session &&) = delete;

        virtual ~Session();

        using id_type = uint64_t;

        // Consume whatever data you can from the buffer. If that's actually none of it, immediately return false.
        // Anything you do consume, call used(n) for it once you no longer need the data (this will destroy buffers).
        virtual task<bool> process(std::string_view const & data) = 0;
        void write(std::string_view data); // Fire and forget writing.
        [[nodiscard]] task<void> flush(std::string_view data = {}); // Awaitable writing.

        task<void> connect(struct sockaddr *, size_t);
        template<typename S>
        auto connect(S * addr) {
            return connect(reinterpret_cast<struct sockaddr *>(addr), sizeof(S));
        }

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
    private:
        id_type m_id;
        Loop & m_loop;
        // this will always be a socket bufferevent.
        struct bufferevent * m_base_buf = nullptr;
        // This might be the same as above, but more likely a TLS (or other filter).
        struct bufferevent * m_top = nullptr;
        std::optional<task<bool>> m_processor;
        sigslot::signal<bool> connected;
        sigslot::signal<bool> written;
    };

    class ListenerBase {
    public:
        ListenerBase(Loop & loop, unsigned short port);

        const struct sockaddr * sockaddr() const;
        void session_connected(evutil_socket_t sock, struct sockaddr * addr, int len);

        virtual void create_session(evutil_socket_t) = 0;

    protected:
        Loop & m_loop;
        unsigned short m_port;
        struct sockaddr_storage m_addr;
    };
}


#endif //COVENT_CORE_H
