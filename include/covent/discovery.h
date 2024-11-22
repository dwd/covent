//
// Created by dwd on 11/14/24.
//

#ifndef COVENT_DISCOVERY_H
#define COVENT_DISCOVERY_H

#include <string>
#include <vector>
#include <list>
#include <covent/dns.h>

namespace covent::dns {
    struct PkixData {
        std::vector<std::string> hostnames;
        std::vector<rr::TLSA> tlsa;
    };

    struct ConnectionTarget {
        struct sockaddr_storage;
        bool tls;
    };

    struct ConnectionPriorityGroup {
        std::vector<ConnectionTarget> targets;
    };

    struct ConnectionData {
        std::list<ConnectionPriorityGroup> priority_groups;
    };

    struct Discovery {
        PkixData pkix_data;
        ConnectionData connection;
    };

    covent::instant_task<PkixData> pkixdata(Resolver & res, std::string const & service, std::string const & hostname, uint16_t port=0);
}

#endif //COVENT_DISCOVERY_H
