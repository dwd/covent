//
// Created by dwd on 12/26/24.
//

#ifndef SERVICE_H
#define SERVICE_H

#include <covent/listener.h>
#include <covent/pkix.h>
#include <spdlog/logger.h>

namespace covent {
    enum class Preference {
        Yes,
        DontCare,
        No
    };

    class ConnectInfo {
    public:
        enum class  Method {
            StartTLS,
            DirectTLS,
            Websocket
        };
        Method method;
        std::string hostname;
        struct sockaddr_storage sockaddr;
        uint16_t port;
        uint16_t priority; // Priority - do not start any with higher priority value until all those with lower have been exhausted.
        uint16_t weight; // Should be used for random-selection within priority bands.
    };

    class GatheredData {
    public:
        std::set<std::string, std::less<>> gathered_hosts; // verified possible hostnames.
        std::list<ConnectInfo> gathered_connect; // Connection options, preference order.
        std::list<covent::dns::rr::TLSA> gathered_tlsa; // Verified TLSA records as gathered.
    };

    class Service {
      public:
        Service();
        Service(Service const &) = delete;
        Service(Service &&) = delete;

        void add(std::unique_ptr<ListenerBase> &&);

        void add(std::unique_ptr<pkix::TLSContext> &&);
        void add(std::unique_ptr<pkix::TLSContext> &&, std::string const & domain);
        void add(std::unique_ptr<dns::Resolver> &&);
        void add(std::unique_ptr<dns::Resolver> &&, std::string const & domain);

        [[nodiscard]] pkix::TLSContext & tls_context(std::string const & domain) const;
        [[nodiscard]] dns::Resolver & resolver(std::string const & domain) const;

        // Do DNS discovery:
        [[nodiscard]] task<GatheredData> discovery(std::string const &) const;

    private:
        [[nodiscard]] task<void> discover_host(dns::Resolver &, GatheredData &, const covent::dns::rr::SRV &, ConnectInfo::Method) const;
        [[nodiscard]] task<void> discover_tlsa(dns::Resolver &, GatheredData &, std::string, uint16_t) const;

        std::shared_ptr<spdlog::logger> m_logger;
        std::map<std::string, std::unique_ptr<dns::Resolver>, std::less<>> m_resolvers;
    };
}

#endif //SERVICE_H
