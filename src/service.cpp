//
// Created by dwd on 12/30/24.
//

#include <covent/app.h>
#include <covent/service.h>
#include <covent/gather.h>

using namespace covent;

Service::Service() {
    m_logger = Application::application().logger("service");
}

void Service::add(std::unique_ptr<dns::Resolver> && resolver) {
    add(std::move(resolver), "");
}

void Service::add(std::unique_ptr<dns::Resolver> && resolver, std::string const & domain) {
    m_resolvers.emplace(domain, std::move(resolver));
}

dns::Resolver & Service::resolver(std::string const & req_domain) const {
    auto domain = req_domain;
    if (auto it = m_resolvers.find(domain); it != m_resolvers.end()) {
        return *it->second;
    }
    for (;;) {
        if (auto dot = domain.find('.'); dot != std::string::npos) {
            domain = domain.substr(dot + 1);
        } else {
            break;
        }
        if (auto it = m_resolvers.find("*." + domain); it != m_resolvers.end()) {
            return *it->second;
        }
    }
    if (auto it = m_resolvers.find(""); it != m_resolvers.end()) {
        return *it->second;
    }
    return covent::Loop::main_loop().default_resolver();
}

task<GatheredData> Service::discovery(std::string const & req_domain) const {
    m_logger->info("Gathering discovery data for {}", req_domain);
    auto & r(resolver(req_domain));
    std::string domain = req_domain;
    GatheredData g;
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
    g.gathered_connect.clear();
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
            co_await discover_host(r, g, srv_rr, method);
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
                co_await discover_host(r, g, rr, ConnectInfo::Method::StartTLS);
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
                co_await discover_host(r, g, rr, ConnectInfo::Method::DirectTLS);
                if (dnssec) co_await discover_tlsa(r, g, rr.hostname, rr.port);
            }
        }
        if (!has_srv && !has_srv_tls) {
            covent::dns::rr::SRV rr;
            rr.hostname = domain;
            rr.port = 5269;
            rr.priority = 1;
            rr.weight = 1;
            co_await discover_host(r, g, rr, ConnectInfo::Method::StartTLS);
            if (dnssec) co_await discover_tlsa(r, g, domain, 5269);
        }
        // TODO : Do SRV sorting here.
    }
//    span->containing_transaction().tag("gather.dnssec", dnssec ? "yes" : "no");
    co_return g;
}


task<void> Service::discover_host(dns::Resolver & r, GatheredData & g, const dns::rr::SRV & rr, ConnectInfo::Method method) const {
    auto [addr_recs4, addr_recs6] = co_await covent::gather(
        r.address_v4(rr.hostname),
        r.address_v6(rr.hostname)
        );
    auto & addr_recs = addr_recs4.error.empty() ? addr_recs4 : addr_recs6;
    if (!addr_recs.error.empty()) co_return; // Interesting case: a DNSSEC-signed SVCB/SRV record pointing to a non-existent host still adds that host to the X.509-acceptable names.
    if (addr_recs4.error.empty() && addr_recs6.error.empty()) {
        for (auto const & rr : addr_recs6.addr) {
            addr_recs4.addr.push_back(rr);
        }
    }
    for (auto const & arr : addr_recs.addr) {
        ConnectInfo conn_info;
        conn_info.method = method;
        conn_info.port = rr.port;
        conn_info.sockaddr = arr;
        conn_info.hostname = rr.hostname;
        if (conn_info.sockaddr.ss_family == AF_INET) {
            //            span->containing_transaction().tag("gather.ipv4", "yes");
            covent::sockaddr_cast<AF_INET>(&conn_info.sockaddr)->sin_port = htons(rr.port);
        } else if (conn_info.sockaddr.ss_family == AF_INET6) {
            //            span->containing_transaction().tag("gather.ipv6", "yes");
            covent::sockaddr_cast<AF_INET6>(&conn_info.sockaddr)->sin6_port = htons(rr.port);
        }
        g.gathered_connect.push_back(conn_info);
    }
}

task<void> Service::discover_tlsa(dns::Resolver & r, GatheredData & g, std::string host, uint16_t port) const {
    auto recs = co_await r.tlsa(port, host);
    if (!recs.error.empty()) co_return;
    if (!recs.dnssec) co_return;
    //    span->containing_transaction().tag("gather.tlsa", "yes");
    for (auto const & tlsa_rr : recs.rrs) {
        g.gathered_tlsa.push_back(tlsa_rr);
    }
}
