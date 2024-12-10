//
// Created by dwd on 11/12/24.
//

#ifndef COVENT_HTTP_H
#define COVENT_HTTP_H

#include <string_view>
#include <utility>
#include <event2/http.h>

#include "coroutine.h"
#include "future.h"
#include "dns.h"
#include "pkix.h"

struct evhttp_request;
struct evhttp;
struct evkeyvalq;
struct event_base;
struct bufferevent;

namespace covent {
    namespace http {
        class ConstFieldRef {
        public:
            ConstFieldRef(evkeyvalq *, std::string const & field);

            std::string_view const &field() const;
            operator std::string_view() const;
            operator bool() const;
            auto operator <=> (std::string_view const & o) const {
                std::string_view me = (*this);
                return me <=> o;
            }
            auto operator == (std::string_view const & o) const {
                std::string_view me = (*this);
                return me == o;
            }
            auto operator != (std::string_view const & o) const {
                std::string_view me = (*this);
                return me != o;
            }

        protected:
            std::string const & m_field;
            evkeyvalq * m_header;
        };
        class FieldRef : public ConstFieldRef {
        public:
            using ConstFieldRef::ConstFieldRef;

            FieldRef & operator = (std::string const &);
        };

        class Request;
        class Response {
        public:
            explicit Response(evhttp_request *);
            ~Response();

            [[nodiscard]] int status() const;

            FieldRef operator [] (std::string const &);
            ConstFieldRef operator [] (std::string const &) const;
            auto const & body() const {
                return m_body;
            }
        private:
            [[nodiscard]] std::string_view body_low() const;

            evhttp_request * m_request;
            std::string m_body;
        };
        class Request {
        public:
            enum class Method {
                GET, POST, PUT, DELETE
            };
            Request(Method, std::string uri);
            Request(covent::dns::Resolver &, Method, std::string uri);
            Request(covent::pkix::PKIXValidator &, covent::dns::Resolver &, Method, std::string uri);
            ~Request();
            FieldRef operator [] (std::string const &);
            ConstFieldRef operator [] (std::string const &) const;
            covent::task<Response> operator () ();

            void complete();

            std::string const & uri() const {
                return m_uri;
            }
            Method const method;
        private:
            covent::dns::Resolver & m_resolver;
            // covent::pkix::PKIXValidator & m_validator;
            covent::pkix::TLSContext & m_tls_context;
            std::string m_uri;
            evhttp_request * m_request;
            covent::future<void> m_completed;
        };
        class Endpoint {
        public:
            Endpoint(std::string const & path);

            template<typename T>
            requires std::is_invocable_r_v<covent::task<int>, T, evhttp_request *>
            Endpoint(std::string  path, T && t): m_path(std::move(path)), m_handler(t) {}

            covent::task<int> handler_low(struct evhttp_request * req);
            virtual covent::task<int> handler(struct evhttp_request * req);
            void add(std::unique_ptr<Endpoint> && child);
            auto const & path() const {
                return m_path;
            }

        private:
            const std::string m_path;
            std::map<std::string, std::unique_ptr<Endpoint>, std::less<>> m_endpoints;
            std::function<covent::task<int>(evhttp_request *)> m_handler;
        };
        class Server {
        public:
            Server(short unsigned int port, covent::pkix::TLSContext & tls_context);
            ~Server();

            Endpoint & root() const {
                return *m_root;
            }

            void add(std::unique_ptr<Endpoint> && endpoint);
            covent::task<void> handler_low(struct evhttp_request * req);
			struct bufferevent * get_buffer_event(struct event_base *);
			void request_handler(struct evhttp_request * req);
        private:
            covent::pkix::TLSContext & m_tls_context;
            std::unique_ptr<Endpoint> m_root;
            struct evhttp * m_server;
            std::list<covent::task<void>> m_in_flight;
        };
    }
}

#endif //COVENT_HTTP_H
