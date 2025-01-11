//
// Created by dwd on 11/12/24.
//
#include <ranges>
#include <algorithm>

#include "covent/http.h"

using namespace covent::http;


ConstFieldRef::ConstFieldRef(Header & header, const std::string &field) : m_field(field), m_header(header) {
    std::ranges::transform(m_field, m_field.begin(), [](auto c) { return std::tolower(c); });
}

ConstFieldRef::operator std::string_view() const {
    auto field_value =  m_header.find(m_field);
    if (field_value == m_header.end()) {
        throw std::runtime_error("Field not found");
    }
    return field_value->second;
}

ConstFieldRef::operator bool() const {
    return m_header.contains(m_field);
}

FieldRef & FieldRef::operator=(const std::string & value) {
    m_header[m_field] = value;
    return *this;
}

