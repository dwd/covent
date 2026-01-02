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
#include "service.h"

struct evhttp_request;
struct evhttp;
struct evkeyvalq;
struct event_base;
struct bufferevent;


    namespace covent::http {
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

            explicit URI(std::string_view text);
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

            [[nodiscard]] std::string_view const &field() const;
            operator std::string_view() const; // NOLINT
            operator bool() const; // NOLINT
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
            [[nodiscard]] std::string_view body() const;
        private:
            std::unique_ptr<Message> m_response;
        };
        class Client  {
        public:
            Client(Service & service, URI const & uri);
            Client(Service & service, std::string_view uri);

            Request request(Method, URI const & uri, std::optional<std::string> body = {});
            Request request(Method, std::string_view uri, std::optional<std::string> body = {});

            covent::task<std::unique_ptr<Response>> send(Request const &);

        private:
            class HTTPSession;
            [[nodiscard]] covent::task<std::shared_ptr<HTTPSession>> connect(dns::answers::Address address, URI const & uri) const;
            [[nodiscard]] covent::task<std::shared_ptr<HTTPSession>> connect_v4(URI const & uri) const;
            [[nodiscard]] covent::task<std::shared_ptr<HTTPSession>> connect_v6(URI const & uri) const;

            Loop & m_loop;
            Service & m_service;
            URI m_uri;
            std::unique_ptr<Session> m_session;
            std::shared_ptr<spdlog::logger> m_log;
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
            explicit Middleware(T && t) : m_handler(t) {};

            virtual covent::task<void> handle(struct evhttp_request * req);
            virtual ~Middleware() = default;
        private:
            std::function<covent::task<void>(evhttp_request *)> m_handler;
        };
        namespace detail {
            struct TemplateSegment {
                enum Type { Literal, Variable } type;
                std::string name;      // variable name
                std::string conv;      // converter name ("int", "str", "bool", "path")
                std::string literal;   // literal text
            };
            struct CompiledTemplate {
                std::string raw;
                std::vector<TemplateSegment> segments;
            };
            CompiledTemplate compileTemplate(const std::string& templ);
        }
        class Endpoint {
        public:
            virtual ~Endpoint() = default;

            explicit Endpoint(std::string path);
            Endpoint(Method method, std::string path);

            template<typename T>
            requires std::is_invocable_r_v<covent::task<int>, T, evhttp_request *>
            Endpoint(std::string path, T && t): m_path(std::move(path)), m_path_template(detail::compileTemplate(m_path)), m_handler([t](evhttp_request * req, std::unordered_map<std::string,std::string> const &){ return t(req);}) {}

            template<typename T>
            requires std::is_invocable_r_v<covent::task<int>, T, evhttp_request *, std::unordered_map<std::string,std::string> const &>
            Endpoint(std::string path, T && t): m_path(std::move(path)), m_path_template(detail::compileTemplate(m_path)), m_handler(t) {}

            covent::task<int> handler_low(struct evhttp_request * req, std::unordered_map<std::string, std::string> const &);
            virtual covent::task<int> handler(struct evhttp_request * req, std::unordered_map<std::string, std::string> const &);
            void add(std::unique_ptr<Endpoint> && child);
            void add(std::unique_ptr<Middleware> && child);
            [[nodiscard]] auto const & path() const {
                return m_path;
            }
            [[nodiscard]] auto const & path_template() const {
                return m_path_template;
            }

        private:
            const std::string m_path;
            const detail::CompiledTemplate m_path_template;
            std::list<std::unique_ptr<Endpoint>> m_endpoints;
            std::list<std::unique_ptr<Middleware>> m_middleware;
            std::function<covent::task<int>(evhttp_request *, std::unordered_map<std::string, std::string> const &)> m_handler;
        };
        class Server {
        public:
            explicit Server(short unsigned int port, bool tls);
            ~Server();

            [[nodiscard]] Endpoint & root() const {
                return *m_root;
            }
            [[nodiscard]] Service & service() const {
                return *m_service;
            }

            void add(std::unique_ptr<Endpoint> && endpoint);
            covent::task<void> handler_low(struct evhttp_request * req);
			struct bufferevent * get_buffer_event(struct event_base *);
			void request_handler(struct evhttp_request * req);
        private:
            std::unique_ptr<Service> m_service;
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
                [[nodiscard]] auto status() const {
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


#endif //COVENT_HTTP_H
