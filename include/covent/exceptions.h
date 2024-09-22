//
// Created by dwd on 9/22/24.
//

#ifndef COVENT_EXCEPTIONS_H
#define COVENT_EXCEPTIONS_H

#include <stdexcept>

namespace covent {
    class covent_logic_error : public std::logic_error {
        using std::logic_error::logic_error;
    };
    class covent_runtime_error : public std::runtime_error {
        using std::runtime_error::runtime_error;
    };
}

#endif //COVENT_EXCEPTIONS_H
