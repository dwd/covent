//
// Created by dwd on 2/23/25.
//

#ifndef SENTRY_H
#define SENTRY_H

#include <sentry.h>
#include <memory>
#include <optional>

namespace covent::sentry {
    class span;
    class transaction;

    class span : public std::enable_shared_from_this<span> {
        sentry_span_t *  m_span = nullptr;
        std::shared_ptr<transaction> m_trans;
        std::weak_ptr<span> m_parent;


        void end();
    public:
        span(sentry_span_t * s, std::shared_ptr<transaction> t);
        span(sentry_span_t * s, std::shared_ptr<span> const & parent, std::shared_ptr<transaction> t);
        span(span const &) = delete;
        span(span &&) = delete;
        sentry::transaction  & containing_transaction() const {
            return *m_trans;
        }
        std::shared_ptr<span> parent() const;
        std::shared_ptr<span> start_child(std::string const & op_name, std::string const & desc);
        static std::shared_ptr<span> start(std::string const & op_name, std::string const & desc);

        void terminate();
        void exception(std::exception_ptr const &);
        ~span();
    };

    class transaction : public std::enable_shared_from_this<transaction> {
        sentry_transaction_t * m_trans;
        sentry_transaction_context_t * m_trans_ctx;

        void end();
    public:
        transaction(std::string const & op_name, std::string const & description, std::optional<std::string> const & trace_header = {});
        static std::shared_ptr<transaction> start(std::string const & op_name, std::string const & description, std::optional<std::string> const & trace_header = {});
        void tag(std::string_view const &, std::string_view const &);
        void name(std::string const &);
        std::shared_ptr<span> start_child(std::string const & op_name, std::string const & desc);

        void terminate();
        void exception(std::exception_ptr const &);
        ~transaction();
    };

}


#endif //SENTRY_H
