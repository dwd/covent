//
// Created by dwd on 11/19/24.
//

#ifndef COVENT_TEMP_H
#define COVENT_TEMP_H

#include <concepts>
#include <optional>

// A bit like an optional, except it can hold a reference, a copy, or a pointer.

namespace covent {
    template<typename T> concept reference = std::is_reference_v<T>;
    template<typename T> concept refreference = std::is_rvalue_reference_v<T>;
    template<typename T> concept pointer = std::is_pointer_v<T>;
    template<typename T> concept move = !std::is_copy_constructible_v<T> && std::is_move_constructible_v<T> && !std::is_pointer_v<T> && !std::is_reference_v<T> && !std::is_rvalue_reference_v<T>;
    template<typename T> concept copy = std::is_copy_constructible_v<T> && !std::is_move_constructible_v<T> && !std::is_pointer_v<T> && !std::is_reference_v<T> && !std::is_rvalue_reference_v<T>;

    template<typename V>
    class temp {
    public:
        using value_type = V;

        void assign(value_type v) {
            m_value.emplace(v);
        }
        auto & operator = (value_type v) {
            assign(v);
            return *this;
        }

        value_type value() {
            return m_value.value();
        }
    private:
        std::optional<value_type> m_value;
    };

    template<move V>
    class temp<V> {
    public:
        using value_type = V;

        void assign(value_type && v) {
            m_value.emplace(std::move(v));
        }
        auto & operator = (value_type && v) {
            assign(v);
            return *this;
        }

        value_type value() {
            return std::move(m_value.value());
        }
    private:
        std::optional<value_type> m_value;
    };

    template<copy V>
    class temp<V> {
    public:
        using value_type = V;

        void assign(value_type const & v) {
            m_value.emplace(v);
        }
        auto & operator = (value_type const & v) {
            assign(v);
            return *this;
        }

        value_type const & value() {
            return m_value.value();
        }
    private:
        std::optional<value_type> m_value;
    };

    template<reference V>
    class temp<V> {
    public:
        using value_type = std::remove_reference_t<V>;

        void assign(value_type & v) {
            m_value = &v;
        }
        auto & operator = (value_type & v) {
            assign(v);
            return *this;
        }

        value_type  & value() {
            return *m_value;
        }
    private:
        value_type * m_value = nullptr;
    };

    template<pointer V>
    class temp<V> {
    public:
        using value_type = std::remove_pointer_t<V>;

        void assign(value_type * v) {
            m_value = v;
        }
        auto & operator = (value_type * v) {
            assign(v);
            return *this;
        }

        value_type  * value() {
            return m_value;
        }
    private:
        value_type * m_value = nullptr;
    };

    template<>
    class temp<void> {
        bool m_value = false;

    public:
        using value_type = void;
        void assign() {
            m_value = true;
        }

        void value() {}
    };
}

#endif //COVENT_TEMP_H
