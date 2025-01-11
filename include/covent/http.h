//
// Created by dwd on 11/12/24.
//

#ifndef COVENT_HTTP_H
#define COVENT_HTTP_H

#include <string_view>
#include <tuple>
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
        using Header = std::map<std::string, std::string, std::less<>>;
        enum class Method {
            GET, POST, PUT, DELETE
        };
        class URI {
        public:
            std::string scheme;
            std::optional<uint16_t> port;
            std::string host;
            std::string netloc;
            std::string path;

            URI(std::string_view text);
        private:
            void parse(std::string_view text);
        };

        class Message {
        public:
            Message() = default;
            explicit Message(URI const & u);
            explicit Message(std::string_view u);
            Header header;

            int status_code = -1; // response
            std::string status_text;

            Method method = Method::GET; // request
            std::optional<URI> uri;

            std::string body;
            bool complete = false;
            bool request = false;

            unsigned long process(std::string_view data);
            [[nodiscard]] std::string render_request() const;
            [[nodiscard]] std::string render_header() const;
            [[nodiscard]] std::string render_body() const;
        private:
            bool header_read = false;
            bool chunked = false;
            std::optional<std::size_t> body_remaining;

            unsigned long process_status(std::string_view & data);
            unsigned long process_request(std::string_view & data);
            unsigned long process_header(std::string_view & data);

            void check_body_type();

            unsigned long read_body(std::string_view &data);

            std::tuple<unsigned long, unsigned long> read_body_chunk_header(std::string_view &data);

            unsigned long read_body_chunk_normal(std::string_view &data, unsigned long chunk_len);

            unsigned long read_body_chunk_final(std::string_view &data);

            unsigned long read_body_chunked(std::string_view &data);
        };

        class ConstFieldRef {
        public:
            ConstFieldRef(Header & header, std::string const & field);

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
            std::string m_field;
            Header & m_header;
        };
        class FieldRef : public ConstFieldRef {
        public:
            using ConstFieldRef::ConstFieldRef;

            FieldRef & operator = (std::string const &);
        };

        class Request;
        class Client;
        class Response {
        public:
            explicit Response(std::unique_ptr<Message> &&);

            [[nodiscard]] int status() const;

            FieldRef operator [] (std::string const &);
            ConstFieldRef operator [] (std::string const &) const;
            std::string_view body() const;
        private:
            std::unique_ptr<Message> m_response;
        };
        class Client {
        public:
            Client(covent::dns::Resolver &, covent::pkix::PKIXValidator &, covent::pkix::TLSContext &, URI const & uri);
            Client(covent::dns::Resolver &, covent::pkix::PKIXValidator &, covent::pkix::TLSContext &, std::string_view uri);

            Request request(Method, URI const & uri, std::optional<std::string> body = {});
            Request request(Method, std::string_view uri, std::optional<std::string> body = {});

            covent::task<std::unique_ptr<Response>> send(Request &);

        private:
            Loop & m_loop;
            URI m_uri;
            std::unique_ptr<Session> m_session;
            covent::dns::Resolver & m_resolver;
            covent::pkix::PKIXValidator & m_validator;
            covent::pkix::TLSContext & m_tls_context;
        };
        class Request {
        public:
            Request(Method, URI const & uri);
            Request(Method, std::string_view uri);
            Request(Client &, Method, URI const & uri);
            FieldRef operator [] (std::string const &);
            ConstFieldRef operator [] (std::string const &) const;
            covent::task<std::unique_ptr<Response>> operator () ();

            void send(Session &sess) const;

            void complete();

            URI const & uri() const {
                return m_request->uri.value();
            }
            Method const method;
        private:
            Loop & m_loop;
            std::unique_ptr<Message> m_request;
            std::unique_ptr<Client> m_client_temp;
            Client & m_client;
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
