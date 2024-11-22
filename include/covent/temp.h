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
    template<typename T> concept copymove = (std::is_copy_constructible_v<T> || std::is_move_constructible_v<T>) && !std::is_pointer_v<T> && !std::is_reference_v<T> && !std::is_rvalue_reference_v<T>;

    template<typename V> class temp;

    template<refreference V>
    class temp<V> {
        std::optional<V> m_value;

    public:
        using value_type = V;

        void assign(V v) {
            m_value.emplace(std::move(v));
        }
        auto & operator = (V v) {
            assign(v);
            return *this;
        }

        V value() {
            return m_value.value();
        }
    };

    template<copymove V>
    class temp<V> {
        std::optional<V> m_value;

    public:
        using value_type = V;

        void assign(V v) {
            m_value.emplace(v);
        }
        auto & operator = (V v) {
            assign(v);
            return *this;
        }

        V value() {
            return m_value.value();
        }
    };

    template<reference V>
    class temp<V> {
        using holding_type = std::remove_reference_t<V>;
        holding_type * m_value = nullptr;

    public:
        using value_type = V;
        void assign(V v) {
            m_value = &v;
        }
        auto & operator = (V v) {
            assign(v);
            return *this;
        }

        V value() {
            return *m_value;
        }
    };

    template<pointer V>
    class temp<V> {
        V m_value = nullptr;

    public:
        using value_type = V;
        void assign(V v) {
            m_value = v;
        }
        auto & operator = (V v) {
            assign(v);
            return *this;
        }

        V value() {
            return m_value;
        }
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
