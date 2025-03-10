//
// Created by dwd on 12/26/24.
//

#ifndef SERVICE_H
#define SERVICE_H

#include <covent/listener.h>
#include <covent/pkix.h>
#include <spdlog/logger.h>

#include "generator.h"

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
    };

    class GatheredData {
    public:
        std::set<std::string, std::less<>> gathered_hosts; // verified possible hostnames.
        std::list<covent::dns::rr::TLSA> gathered_tlsa; // Verified TLSA records as gathered.
    };

    class Service {
      public:
        Service();
        Service(Service const &) = delete;
        Service(Service &&) = delete;

        class Entry {
            Entry * m_parent = nullptr;
            Service & m_service;
            std::string m_name;
            std::unique_ptr<dns::Resolver> m_resolver;
            std::unique_ptr<pkix::PKIXValidator> m_validator;
            std::unique_ptr<pkix::TLSContext> m_tls_context;
            std::shared_ptr<spdlog::logger> m_logger;

        public:
            explicit Entry(Service & service, std::string_view name);
            Entry(Service & service, std::string_view name, Entry & parent);

            auto const & name() const {
                return m_name;
            }

            template<typename... Args>
            auto & make_resolver(Args... args) {
                m_resolver = std::make_unique<dns::Resolver>(std::forward<Args...>(args)...);
                return *m_resolver;
            }
            template<typename... Args>
            auto & make_validator(Args... args) {
                m_validator = std::make_unique<pkix::PKIXValidator>(m_service, std::forward<Args>(args)...);
                return *m_validator;
            }
            template<typename... Args>
            auto & make_tls_context(Args... args) {
                m_tls_context = std::make_unique<pkix::TLSContext>(m_service, std::forward<Args>(args)...);
                return *m_tls_context;
            }

            [[nodiscard]] dns::Resolver & resolver() const;
            [[nodiscard]] pkix::PKIXValidator & validator() const;
            [[nodiscard]] pkix::TLSContext & tls_context() const;

            // Do DNS discovery:
            [[nodiscard]] task<GatheredData> discovery(std::string domain) const;
            [[nodiscard]] generator_async<dns::rr::SRV> srv_lookup(std::string domain, std::vector<std::string> services, bool dnssec_only = false) const;

            generator_async<ConnectInfo> xmpp_lookup(std::string domain) const;

            [[nodiscard]] task<void> discover_tlsa(dns::Resolver &, GatheredData &, std::string, uint16_t) const;
        };

        void add(std::unique_ptr<ListenerBase> &&);

        [[nodiscard]] Entry & entry(std::string const & domain) const;
        Entry & add(std::string_view name);
        Entry & add(std::string_view name, Entry & parent);
        Entry & add(std::unique_ptr<Entry> && entry);

    private:
        std::shared_ptr<spdlog::logger> m_logger;
        std::map<std::string, std::unique_ptr<Entry>, std::less<>> m_entries;
    };
}

#endif //SERVICE_H
