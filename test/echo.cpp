//
// Created by dwd on 8/26/24.
//

#include "gtest/gtest.h"
#include <covent/covent.h>
#include <covent/loop.h>

namespace echo {
    class ServerSession : public covent::Session {
    public:
        ServerSession(covent::Loop & l) : covent::Session(l) {}

        covent::task<size_t> read(std::string_view & data) override {
            this->write(data);
            data.remove_suffix(data.length());
            co_return data.length();
        }

        void closed() {
            // Other side has closed (but not, perhaps, us yet!)
            loop().shutdown();
        }
    };
}

TEST(Echo, listen) {
    covent::Loop serverLoop;
    covent::Listener<echo::ServerSession> listener(serverLoop, 2007);
}
