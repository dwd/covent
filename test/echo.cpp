//
// Created by dwd on 8/26/24.
//

#include "gtest/gtest.h"
#include <covent/covent.h>
#include <covent/loop.h>
#include <thread>

namespace echo {
    const std::string test_data = "This is the data I'm going to send";
    std::string data_rx_server;
    std::string data_rx_client;

    class ServerSession : public covent::Session {
    public:
        ServerSession(covent::Loop & l, evutil_socket_t sock) : covent::Session(l, sock) {
            std::cout << "Created session" << std::endl;
        }

        covent::task<bool> process(std::string_view const & data) override {
            std::cout << "Got " << data.length() << " bytes" << std::endl;
            data_rx_server = data;
            co_await this->flush(data);
            used(data.length());
            co_return true;
        }
    };
    class ClientSession : public covent::Session {
    public:
        covent::Loop & other;
        ClientSession(covent::Loop & l, covent::Loop & o) : other(o), covent::Session(l) {}

        covent::task<bool> process(std::string_view const & data) override {
            loop().shutdown();
            std::cout << "Echo " << data.length() << " bytes" << std::endl;
            data_rx_client = data;
            used(data.length());
            loop().defer([]() {
                covent::Loop::thread_loop().shutdown();
            });
            other.defer([&other = other]() {
                other.shutdown();
            });
            co_return true;
        }
    };
}

TEST(Echo, listen) {
    {
        covent::Loop serverLoop;
        covent::Listener<echo::ServerSession> listener(serverLoop, 2007);
        serverLoop.listen(listener);
        std::jthread foo{
                [&serverLoop]() {
                    try {
                        std::cout << "Thread..." << std::endl;
                        covent::Loop clientLoop;
                        auto &cl = clientLoop.add(std::make_unique<echo::ClientSession>(clientLoop, serverLoop));
                        struct sockaddr_in6 sin6 {
                            .sin6_family = AF_INET6,
                            .sin6_port = htons(2007),
                            .sin6_addr = IN6ADDR_LOOPBACK_INIT,
                        };
                        auto task = cl.connect(&sin6);
                        std::cout << "Connect start" << std::endl;
                        if (!task.start()) {
                            std::cout << "Waiting for connect" << std::endl;
                            clientLoop.run_until([&task]() { return task.done(); });
                        }
                        std::cout << "Connect end: " << task.done() << std::endl;
                        task.get();
                        cl.write(echo::test_data);
                        clientLoop.run();
                        std::cout << "Probably written?" << std::endl;
                    } catch (std::exception &e) {
                        std::cout << "Boom: " << e.what() << std::endl;
                    } catch (...) {
                        std::cout << "Big boom" << std::endl;
                    }
                }
        };
        std::cout << "Main loop start" << std::endl;
        serverLoop.run();
    }
    EXPECT_EQ(echo::test_data, echo::data_rx_server);
    EXPECT_EQ(echo::test_data, echo::data_rx_client);
}
