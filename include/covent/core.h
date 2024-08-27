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

struct bufferevent;

namespace covent {
    class Session {
    public:
        Session(Loop & loop);
        Session(Loop & loop, evutil_socket_t sock);
        Session() = delete;
        Session(Session const &) = delete;
        Session(Session &&) = delete;

        using id_type = uint64_t;

        virtual task<size_t> read(std::string_view & data) = 0;
        void write(std::string_view data);

        task<void> connect(struct sockaddr *);

        id_type id() const {
            return m_id;
        }

        Loop & loop() const {
            return m_loop;
        }

    private:
        void read_cb(struct bufferevent * bev);
        void write_cb(struct bufferevent * bev);
        void event_cb(struct bufferevent * bev, short flags);
        id_type m_id;
        Loop & m_loop;
        std::unique_ptr<struct bufferevent, std::function<void(struct bufferevent *)>> m_buf;
        struct bufferevent * m_top = nullptr;
    };

    class ListenerBase {
    public:
        ListenerBase(Loop & loop, unsigned short port);

        const struct sockaddr * sockaddr() const;
        void session_connected(evutil_socket_t sock, struct sockaddr * addr, int len);

        virtual void create_session() = 0;

    protected:
        Loop & m_loop;
        unsigned short m_port;
        struct sockaddr_storage m_addr;
    };
}


#endif //COVENT_CORE_H
