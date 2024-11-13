//
// Created by dwd on 11/12/24.
//

#ifndef COVENT_HTTP_H
#define COVENT_HTTP_H

#include <string_view>
#include "coroutine.h"
#include "future.h"

struct evhttp_request;
struct evkeyvalq;

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

        class Response {
        public:
            Response();

            FieldRef & operator [] (std::string_view const &);
            FieldRef const & operator [] (std::string_view const &) const;
            std::string const & body() const;
        };
        class Request {
        public:
            enum class Method {
                GET, POST, PUT, DELETE
            };
            Request(Method, std::string uri);
            FieldRef operator [] (std::string const &);
            ConstFieldRef operator [] (std::string const &) const;
            covent::task<Response> operator () () const;

            void complete();

            std::string_view uri() const;
            Method const method;
        private:
            std::string m_uri;
            evhttp_request * m_request;
            covent::future<void> m_completed;
        };
    }
}

#endif //COVENT_HTTP_H
