//
// Created by dwd on 11/12/24.
//
#include <event2/http.h>

#include "covent/http.h"

using namespace covent::http;


ConstFieldRef::ConstFieldRef(evkeyvalq * header, const std::string &field) : m_header(header), m_field(field) {
}

ConstFieldRef::operator std::string_view() const {
    auto field_value =  evhttp_find_header(m_header, m_field.c_str());
    if (field_value == nullptr) {
        throw std::runtime_error("Field not found");
    }
    return field_value;
}

ConstFieldRef::operator bool() const {
    auto field_value =  evhttp_find_header(m_header, m_field.c_str());
    return field_value != nullptr;
}

FieldRef & FieldRef::operator=(const std::string & value) {
    if (0 > evhttp_add_header(m_header, m_field.c_str(), value.c_str())) {
        throw std::runtime_error("Failed to add header field");
    }
    return *this;
}

