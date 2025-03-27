//
// Created by dwd on 8/26/24.
//

#include "gtest/gtest.h"
#include <covent/covent.h>
#include <covent/loop.h>
#include <thread>
#include <covent/sleep.h>

namespace echo {
    const std::string test_data = "This is the data I'm going to send\r\nAnd this is more.";
    std::string data_rx_server;
    std::string data_rx_client;

    class ServerSession : public covent::Session {
    public:
        ServerSession(covent::Loop & l, evutil_socket_t sock, covent::ListenerBase & b) : covent::Session(l, sock, b) {
            std::cout << "Created server session" << std::endl;
        }
        ~ServerSession() {
            std::cout << "Destroyed server session" << std::endl;
        }

        covent::task<std::size_t> process(std::string_view data) override {
            std::cout << "Got " << data.length() << " bytes" << std::endl;
            data_rx_server = data;
            co_await covent::sleep(0.1);
            co_await this->flush(data);
            std::cout << "Flushed " << data.length() << " bytes" << std::endl;
            co_return data.length();
        }
    };
    class BrokenServerSession : public covent::Session {
    public:
        BrokenServerSession(covent::Loop & l, evutil_socket_t sock, covent::ListenerBase & b) : covent::Session(l, sock, b) {
            std::cout << "Created server session" << std::endl;
        }
        ~BrokenServerSession() {
            std::cout << "Destroyed server session" << std::endl;
        }

        covent::task<std::size_t> process(std::string_view data) override {
            std::cout << "Got " << data.length() << " bytes" << std::endl;
            co_await covent::sleep(0.1);
            data_rx_server = data;
            throw std::runtime_error("Broken");
        }

        void closed() override {
            Session::closed();
            loop().defer([]() {
                covent::Loop::thread_loop().shutdown();
            }, 0.3);
        }
    };
    class LineServerSession : public covent::Session {
    public:
        LineServerSession(covent::Loop & l, evutil_socket_t sock, covent::ListenerBase & b) : covent::Session(l, sock, b) {
            std::cout << "Created server session" << std::endl;
        }
        ~LineServerSession() {
            std::cout << "Destroyed server session" << std::endl;
        }

        covent::task<std::size_t> process(std::string_view data) override {
            std::cout << "Got " << data.length() << " bytes" << std::endl;
            data_rx_server = data;
            co_await covent::sleep(0.4);
            if (data.contains("\r\n")) {
                std::cout << "Have CRLF, echoing" << std::endl;
                auto p = data.find("\r\n");
                p += 2;
                write(data.substr(0, p));
                co_return p;
            }
            std::cout << "No CRLF, do nothing yet" << std::endl;
            co_return 0;
        }

        void closed() override {
            loop().shutdown();
        }
    };
    class ClientSession : public covent::Session {
    public:
        covent::Loop & other;
        ClientSession(covent::Loop & l, covent::Loop & o) : covent::Session(l), other(o) {}

        covent::task<std::size_t> process(std::string_view data) override {
            loop().shutdown();
            std::cout << "Echo " << data.length() << " bytes" << std::endl <<std::flush;
            data_rx_client = data;
            loop().defer([]() {
                std::cout << "Shutting down my thread" <<std::endl << std::flush;
                covent::Loop::thread_loop().shutdown();
            });
            other.defer([&other = other]() {
                std::cout << "Shutting down other thread" <<std::endl << std::flush;
                other.shutdown();
            });
            co_return data.length();
        }

        covent::task<void> normal_echo(unsigned short port) {
            struct sockaddr_in6 sin6 {
                    .sin6_family = AF_INET6,
                    .sin6_port = htons(port),
                    .sin6_addr = IN6ADDR_LOOPBACK_INIT,
            };
            co_await connect(&sin6);
            co_await flush(echo::test_data);
        }

        covent::task<void> normal_echo_two(unsigned short port) {
            struct sockaddr_in6 sin6 {
                    .sin6_family = AF_INET6,
                    .sin6_port = htons(port),
                    .sin6_addr = IN6ADDR_LOOPBACK_INIT,
            };
            co_await connect(&sin6);
            co_await flush(echo::test_data + "\r\n");
        }

        covent::task<void> slow_echo(unsigned short port) {
            struct sockaddr_in6 sin6 {
                    .sin6_family = AF_INET6,
                    .sin6_port = htons(port),
                    .sin6_addr = IN6ADDR_LOOPBACK_INIT,
            };
            co_await connect(&sin6);
            for (auto c : echo::test_data) {
                std::cout << "Send a byte" << std::endl;
                write(std::string("") + c);
                co_await covent::sleep(0.1);
            }
        }

        void closed() override {
            close();
            loop().defer([]() {
                std::cout << "Shutting down my thread" <<std::endl << std::flush;
                covent::Loop::thread_loop().shutdown();
            });
            other.defer([&other = other]() {
                std::cout << "Shutting down other thread" <<std::endl << std::flush;
                other.shutdown();
            });
        }
    };
}

TEST(Echo, listen) {
    {
        echo::data_rx_server = "";
        echo::data_rx_client = "";
        covent::Loop serverLoop;
        covent::Listener<echo::ServerSession> listener(serverLoop, "::1", 2007);
        serverLoop.listen(listener);
        std::jthread foo{
                [&serverLoop]() {
                    try {
                        std::cout << "Thread..." << std::endl;
                        covent::Loop clientLoop;
                        auto cl = std::dynamic_pointer_cast<echo::ClientSession>(clientLoop.add(std::make_shared<echo::ClientSession>(clientLoop, serverLoop)));
                        clientLoop.run_task(cl->normal_echo(2007));
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

TEST(Echo, listen_line) {
    {
        echo::data_rx_server = "";
        echo::data_rx_client = "";
        covent::Loop serverLoop;
        covent::Listener<echo::LineServerSession> listener(serverLoop, "::1", 2009);
        serverLoop.listen(listener);
        std::jthread foo{
                [&serverLoop]() {
                    try {
                        std::cout << "Thread..." << std::endl;
                        covent::Loop clientLoop;
                        auto cl = std::dynamic_pointer_cast<echo::ClientSession>(clientLoop.add(std::make_shared<echo::ClientSession>(clientLoop, serverLoop)));
                        clientLoop.run_task(cl->normal_echo(2009));
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
    EXPECT_EQ("This is the data I'm going to send\r\n", echo::data_rx_client);
}


TEST(Echo, listen_line_two) {
    {
        echo::data_rx_server = "";
        echo::data_rx_client = "";
        covent::Loop serverLoop;
        covent::Listener<echo::LineServerSession> listener(serverLoop, "::1", 2009);
        serverLoop.listen(listener);
        std::jthread foo{
                [&serverLoop]() {
                    try {
                        std::cout << "Thread..." << std::endl;
                        covent::Loop clientLoop;
                        auto cl = std::dynamic_pointer_cast<echo::ClientSession>(clientLoop.add(std::make_shared<echo::ClientSession>(clientLoop, serverLoop)));
                        clientLoop.run_task(cl->normal_echo_two(2009));
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
    EXPECT_EQ(echo::test_data + "\r\n", echo::data_rx_server);
    EXPECT_EQ("This is the data I'm going to send\r\n", echo::data_rx_client);
}


TEST(Echo, listen_line_slow) {
    {
        echo::data_rx_server = "";
        echo::data_rx_client = "";
        covent::Loop serverLoop;
        covent::Listener<echo::LineServerSession> listener(serverLoop, "::1", 2010);
        serverLoop.listen(listener);
        std::jthread foo{
                [&serverLoop]() {
                    try {
                        std::cout << "Thread..." << std::endl;
                        covent::Loop clientLoop;
                        auto cl = std::dynamic_pointer_cast<echo::ClientSession>(clientLoop.add(std::make_shared<echo::ClientSession>(clientLoop, serverLoop)));
                        clientLoop.run_task(cl->slow_echo(2010));
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
    // EXPECT_TRUE(echo::test_data.starts_with(echo::data_rx_server));
    EXPECT_EQ("This is the data I'm going to send\r\n", echo::data_rx_client);
}

TEST(Echo, listen_broken) {
    {
        echo::data_rx_server = "";
        echo::data_rx_client = "";
        covent::Loop serverLoop;
        covent::Listener<echo::BrokenServerSession> listener(serverLoop, "::1", 2008);
        serverLoop.listen(listener);
        std::jthread foo{
                [&serverLoop]() {
                    try {
                        std::cout << "Thread..." << std::endl;
                        covent::Loop clientLoop;
                        auto cl = std::dynamic_pointer_cast<echo::ClientSession>(clientLoop.add(std::make_shared<echo::ClientSession>(clientLoop, serverLoop)));
                        clientLoop.run_task(cl->normal_echo(2008));
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
        std::cout << "Main loop end" << std::endl;
    }
    EXPECT_EQ(echo::test_data, echo::data_rx_server);
    EXPECT_EQ("", echo::data_rx_client);
}
