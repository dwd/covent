//
// Created by dwd on 12/30/24.
//

#include <ranges>
#include <random>
#include <covent/app.h>
#include <covent/service.h>
#include <covent/gather.h>
#include <covent/generator.h>

using namespace covent;

Service::Service() {
    m_logger = Application::application().logger("service");
}

Service::Entry & Service::add(std::unique_ptr<Service::Entry> && entry) {
    auto [it, ok] = m_entries.try_emplace(entry->name(), std::move(entry));
    if (!ok) {
        throw std::runtime_error("Error inserting Entry");
    }
    auto const & [name, new_entry] = *it;
    return *new_entry;
}

Service::Entry & Service::add(std::string_view name) {
    return add(std::make_unique<Service::Entry>(*this, name));
}

Service::Entry & Service::add(std::string_view name, Service::Entry & parent) {
    return add(std::make_unique<Entry>(*this, name, parent));
}

Service::Entry & Service::entry(std::string const & req_domain) const {
    auto domain = req_domain;
    if (auto it = m_entries.find(domain); it != m_entries.end()) {
        return *it->second;
    }
    for (;;) {
        if (auto it = m_entries.find("*." + domain); it != m_entries.end()) {
            return *it->second;
        }
        if (auto dot = domain.find('.'); dot != std::string::npos) {
            domain = domain.substr(dot + 1);
        } else {
            break;
        }
    }
    if (auto it = m_entries.find(""); it != m_entries.end()) {
        return *it->second;
    }
    return const_cast<Service * const>(this)->add("");
}

Service::Entry::Entry(Service & service, std::string_view name) : m_service(service), m_name(name), m_logger(Application::application().logger("entry {}", m_name)) {}

Service::Entry::Entry(Service & service, std::string_view name, Entry &parent) : m_service(service), m_parent(&parent), m_name(name), m_logger(Application::application().logger("entry {}", m_name)) {}

dns::Resolver &Service::Entry::resolver() const {
    if (m_resolver) return *m_resolver;
    if (m_parent) return m_parent->resolver();
    if (!m_name.empty()) return m_service.entry("").resolver();
    return const_cast<Entry * const>(this)->make_resolver(false);
}

pkix::TLSContext &Service::Entry::tls_context() const {
    if (m_tls_context) return *m_tls_context;
    if (m_parent) return m_parent->tls_context();
    if (!m_name.empty()) return m_service.entry("").tls_context();
    return const_cast<Entry * const>(this)->make_tls_context(false, true, "");
}

pkix::PKIXValidator &Service::Entry::validator() const {
    if (m_tls_context) return *m_validator;
    if (m_parent) return m_parent->validator();
    if (!m_name.empty()) return m_service.entry("").validator();
    return const_cast<Entry * const>(this)->make_validator(false, true);
}

task<GatheredData> Service::Entry::discovery(std::string const req_domain) const {
    m_logger->info("Gathering discovery data for {}", m_name);
    auto & r(resolver());
    GatheredData g;
    std::string domain = req_domain;
    //auto span = sentry
//    span->containing_transaction().tag("gather.domain", domain);
//    span->containing_transaction().tag("gather.svcb", "no");
//    span->containing_transaction().tag("gather.srv", "no");
//    span->containing_transaction().tag("gather.dnssec", "no");
//    span->containing_transaction().tag("gather.ipv4", "no");
//    span->containing_transaction().tag("gather.ipv6", "no");
//    span->containing_transaction().tag("gather.tlsa", "no");
//    span->containing_transaction().tag("gather.tls.direct", "no");
//    span->containing_transaction().tag("gather.tls.starttls", "no");
    bool dnssec = true;
aname_restart:
    m_logger->debug("ANAME restart");
    auto svcb = co_await r.svcb("xmpp-server", domain);
    if (svcb.error.empty() && !svcb.rrs.empty()) {
        dnssec = dnssec && svcb.dnssec;
//        span->containing_transaction().tag("gather.svcb", "yes");
        // SVCB pathway
        for (auto const & rr : svcb.rrs) {
            if (svcb.dnssec) {
                g.gathered_hosts.insert(rr.hostname);
            }
            if (rr.priority == 0) {
                domain = rr.hostname;
                goto aname_restart;
            }
            uint16_t  default_port = 443; // Anticipation of WebSocket/WebTransport/BOSH.
            auto method = ConnectInfo::Method::StartTLS;
            if (rr.alpn.empty()) {
                method = ConnectInfo::Method::StartTLS;
//                span->containing_transaction().tag("gather.tls.starttls", "yes");
                default_port = 5269;
            } else if (rr.alpn.contains("xmpp-server")) {
                method = ConnectInfo::Method::DirectTLS;
//                span->containing_transaction().tag("gather.tls.direct", "yes");
                default_port = 5270;
            }
            covent::dns::rr::SRV srv_rr;
            srv_rr.hostname = rr.hostname;
            srv_rr.port =  rr.port ? rr.port : default_port;
            if (dnssec) co_await discover_tlsa(r, g, srv_rr.hostname, srv_rr.port);
        }
    } else {
        // SRV path
        auto [srv, srv_tls] = co_await gather(
            r.srv("xmpp-server", domain), // Interesting case: An SVCB looking resulting in the ANAME case might follow to an SRV lookup.
            r.srv("xmpps-server", domain)
            );
        bool has_srv = (srv.error.empty() && !srv.rrs.empty());
        bool has_srv_tls = (srv.error.empty() && !srv.rrs.empty());
        if (has_srv) {
            dnssec = dnssec && srv.dnssec;
//            span->containing_transaction().tag("gather.srv", "yes");
            for (auto const &rr: srv.rrs) {
                if (srv.dnssec) {
                    g.gathered_hosts.insert(rr.hostname);
                }
//                if (rr.tls) {
//                    span->containing_transaction().tag("gather.tls.direct", "yes");
//                } else {
//                    span->containing_transaction().tag("gather.tls.starttls", "yes");
//                }
                if (dnssec) co_await discover_tlsa(r, g, rr.hostname, rr.port);
            }
        }
        if (has_srv_tls) {
            dnssec = dnssec && srv.dnssec;
//            span->containing_transaction().tag("gather.srv", "yes");
            for (auto const &rr: srv_tls.rrs) {
                if (srv_tls.dnssec) {
                    g.gathered_hosts.insert(rr.hostname);
                }
//                if (rr.tls) {
//                    span->containing_transaction().tag("gather.tls.direct", "yes");
//                } else {
//                    span->containing_transaction().tag("gather.tls.starttls", "yes");
//                }
                if (dnssec) co_await discover_tlsa(r, g, rr.hostname, rr.port);
            }
        }
        if (!has_srv && !has_srv_tls) {
            covent::dns::rr::SRV rr;
            rr.hostname = domain;
            rr.port = 5269;
            rr.priority = 1;
            rr.weight = 1;
            if (dnssec) co_await discover_tlsa(r, g, domain, 5269);
        }
    }
//    span->containing_transaction().tag("gather.dnssec", dnssec ? "yes" : "no");
    co_return g;
}


task<void> Service::Entry::discover_tlsa(dns::Resolver & r, GatheredData & g, std::string host, uint16_t port) const {
    auto recs = co_await r.tlsa(port, host);
    if (!recs.error.empty()) co_return;
    if (!recs.dnssec) co_return;
    //    span->containing_transaction().tag("gather.tlsa", "yes");
    for (auto const & tlsa_rr : recs.rrs) {
        g.gathered_tlsa.push_back(tlsa_rr);
    }
}

generator_async<covent::dns::rr::SRV> Service::Entry::srv_lookup(std::string const domain, std::vector<std::string> const services, bool dnssec_only) const {
    auto & resolver = this->resolver();
    auto srv_list = co_await covent::gather(
            services | std::ranges::views::transform([&domain, &resolver](auto const & arg) {
                return resolver.srv(arg, domain);
            })
    );
    // Collapse all the services into one.
    auto it = srv_list.begin();
    auto srv_def = *it++;
    if (!srv_def.dnssec && dnssec_only) {
        srv_def.rrs.clear();
    }
    for (; it != srv_list.end(); ++it) {
        auto const & srv_tls = *it;
        if (!srv_tls.dnssec && dnssec_only) continue;
        for (auto const & rr : srv_tls.rrs) {
            srv_def.rrs.push_back(rr);
        }
    }
    // Sort all the RRs together.
    std::sort(srv_def.rrs.begin(), srv_def.rrs.end(), [](auto const & lhs, auto const & rhs) {
       return lhs.priority < rhs.priority;
    });
    std::random_device rd;
    std::mt19937 gen(rd());
    while (!srv_def.rrs.empty()) {
        auto prio = srv_def.rrs[0].priority;
        int weight_total = 0;
        for (auto const & rr : srv_def.rrs) {
            if (prio != rr.priority) break;
            weight_total += rr.weight;
        }
        if (weight_total == 0) {
            auto rr = srv_def.rrs[0];
            srv_def.rrs.erase(srv_def.rrs.begin());
            co_yield rr;
        }
        std::uniform_int_distribution distrib(0, weight_total);
        auto weight_sel = distrib(gen);
        for (auto i = srv_def.rrs.begin(); i != srv_def.rrs.end(); ++i) {
            weight_sel -= i->weight;
            if (weight_sel <= 0) {
                auto rr = *i;
                srv_def.rrs.erase(i);
                co_yield rr;
                break;
            }
        }
    }
}

generator_async<ConnectInfo> Service::Entry::xmpp_lookup(std::string const domain) const {
    auto & resolver = this->resolver();
    auto srv = srv_lookup(domain, {"xmpp-server", "xmpps-server"});
    for (auto it = co_await  srv.begin(); it != srv.end(); co_await ++it) {
        auto rr = *it;
        ConnectInfo connect_info;
        connect_info.hostname = rr.hostname;
        connect_info.port = rr.port;
        connect_info.method = (rr.service == "xmpp-server" ? ConnectInfo::Method::StartTLS : ConnectInfo::Method::DirectTLS);
        auto ipv6 = co_await resolver.address_v6(rr.hostname);
        if (ipv6.error.empty()) {
            for (auto const & a : ipv6.addr) {
                connect_info.sockaddr = a;
                co_yield connect_info;
            }
        }
        auto ipv4 = co_await resolver.address_v4(rr.hostname);
        if (ipv4.error.empty()) {
            for (auto const & a : ipv4.addr) {
                connect_info.sockaddr = a;
                co_yield connect_info;
            }
        }
    }
}
