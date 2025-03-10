//
// Created by dwd on 8/2/24.
//

#include "covent/sentry.h"
#include "covent/coroutine.h"
// #include "log.h"

using namespace covent::sentry;

transaction::transaction(const std::string &op_name, const std::string &description, std::optional<std::string> const & trace_header) {
    m_trans_ctx = sentry_transaction_context_new(description.c_str(), op_name.c_str());
    if (trace_header.has_value()) {
        sentry_transaction_context_update_from_header(m_trans_ctx, "sentry-trace", trace_header.value().c_str());
    }
    m_trans = sentry_transaction_start(m_trans_ctx, sentry_value_new_null());
}

void transaction::name(std::string const & n) {
    sentry_transaction_set_name(m_trans, n.c_str());
}

void transaction::tag(const std::string_view & tag, const std::string_view & val) {
    std::string full_tag ="covent.";
    full_tag += tag;
    sentry_transaction_set_tag_n(m_trans, full_tag.data(), full_tag.length(), val.data(), val.length());
}

void transaction::end() {
    if (m_trans) {
        sentry_transaction_finish(m_trans);
        m_trans = nullptr;
    }
}

void transaction::terminate() {
    end();
}

void transaction::exception(std::exception_ptr const & eptr) {
    if (m_trans) {
        sentry_transaction_set_status(m_trans, sentry_span_status_t::SENTRY_SPAN_STATUS_INTERNAL_ERROR);
    }
}

transaction::~transaction() {
    if (m_trans) {
        auto eptr = std::current_exception();
        if (eptr) sentry_transaction_set_status(m_trans, sentry_span_status_t::SENTRY_SPAN_STATUS_INTERNAL_ERROR);
        end();
    }
}

std::shared_ptr<span> transaction::start_child(const std::string &op_name, const std::string &desc) {
    sentry_span_t * span_ptr = sentry_transaction_start_child(m_trans, op_name.c_str(), desc.c_str());
    return std::make_shared<span>(span_ptr, shared_from_this());
}

std::shared_ptr<span> span::start_child(const std::string &op_name, const std::string &desc) {
    sentry_span_t * span_ptr = sentry_span_start_child(m_span, op_name.c_str(), desc.c_str());
    return std::make_shared<span>(span_ptr, shared_from_this(), m_trans);
}

std::shared_ptr<span> span::start(const std::string & op_name, const std::string & desc) {
    auto stack = detail::Stack::thread_stack.lock();
    std::shared_ptr<span> new_span;
    if (stack) {
        auto transaction = stack->transaction.lock();
        if (transaction) {
            auto span = stack->current_top.lock();
            if (span) {
                new_span = span->start_child(op_name, desc);
            } else {
                new_span = transaction->start_child(op_name, desc);
            }
        }
    }
    if (new_span) {
        auto stack = detail::Stack::thread_stack.lock();
        if (stack && stack->current_promise) {
            stack->current_promise->span = new_span;
        }
    }
    return new_span;
}

std::shared_ptr<transaction> transaction::start(const std::string & op_name, const std::string & desc, std::optional<std::string> const & trace_header ) {
    auto trans = std::make_shared<transaction>(op_name, desc);
    auto stack = detail::Stack::thread_stack.lock();
    if (stack) stack->transaction = trans;
    return trans;
}

span::span(sentry_span_t *s, std::shared_ptr<sentry::transaction> t) : m_span(s), m_trans(t) {}
span::span(sentry_span_t *s, std::shared_ptr<sentry::span> const & parent, std::shared_ptr<sentry::transaction> t) : m_span(s), m_parent(parent), m_trans(t) {
}

void span::end() {
    if (m_span) {
        sentry_span_finish(m_span);
        m_span = nullptr;
    }
}

void span::terminate() {
    end();
}

void span::exception(std::exception_ptr const & eptr) {
    if (m_span) {
        sentry_span_set_status(m_span, sentry_span_status_t::SENTRY_SPAN_STATUS_INTERNAL_ERROR);
    }
}

span::~span() {
    if (m_span) {
        auto eptr = std::current_exception();
        if (eptr) sentry_span_set_status(m_span, sentry_span_status_t::SENTRY_SPAN_STATUS_INTERNAL_ERROR);
        end();
    }
}

std::shared_ptr<span> span::parent() const {
    return m_parent.lock();
}

