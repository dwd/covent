//
// Created by dwd on 11/14/24.
//

#include <covent/discovery.h>

using namespace covent::dns;

covent::instant_task<PkixData> host_pkix(Resolver &res, const std::string &hostname, uint16_t port) {
    PkixData pkix;
    pkix.hostnames.push_back(hostname);
    auto tlsa = co_await res.tlsa(port, hostname);
    if (tlsa.error.empty() && tlsa.dnssec) {
        for (auto & tlsa_rr : tlsa.rrs) {
            pkix.tlsa.push_back(tlsa_rr);
        }
    }
    co_return pkix;
}
