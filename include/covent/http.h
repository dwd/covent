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
#include "http.h"
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
            Loop & m_loop;
            covent::dns::Resolver & m_resolver;
            // covent::pkix::PKIXValidator & m_validator;
            covent::pkix::TLSContext & m_tls_context;
            std::string m_uri;
            evhttp_request * m_request;
            covent::future<void> m_completed;
        };
        class Middleware {
        public:
            template<typename T>
            requires std::is_invocable_r_v<covent::task<void>, T, evhttp_request *>
            Middleware(T && t) : m_handler(t) {};

            virtual covent::task<void> handle(struct evhttp_request * req);
            virtual ~Middleware() = default;
        private:
            std::function<covent::task<void>(evhttp_request *)> m_handler;
        };
        class Endpoint {
        public:
            Endpoint(std::string path);

            template<typename T>
            requires std::is_invocable_r_v<covent::task<int>, T, evhttp_request *>
            Endpoint(std::string path, T && t): m_path(std::move(path)), m_handler(t) {}

            covent::task<int> handler_low(struct evhttp_request * req);
            virtual covent::task<int> handler(struct evhttp_request * req);
            void add(std::unique_ptr<Endpoint> && child);
            void add(std::unique_ptr<Middleware> && child);
            auto const & path() const {
                return m_path;
            }

        private:
            const std::string m_path;
            std::map<std::string, std::unique_ptr<Endpoint>, std::less<>> m_endpoints;
            std::list<std::unique_ptr<Middleware>> m_middleware;
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
        namespace exception {
            class base : public std::runtime_error {
            public:
                using std::runtime_error::runtime_error;
            };
            class http_status : public base {
            public:
                http_status(int status, std::string const & what) : m_status(status), base(what) {}
                auto status() {
                    return m_status;
                }

            private:
                int m_status;
            };
            class not_found : public http_status {
            public:
                not_found() : http_status(404, "Not found") {}
                explicit not_found(std::string const & s) : http_status(404, s) {}
            };
            class internal : public http_status {
            public:
                internal() : http_status(500, "Internal Error") {}
                explicit internal(std::string const & s) : http_status(500, s) {}
            };
        }
    }
}

#endif //COVENT_HTTP_H
