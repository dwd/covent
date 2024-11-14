#include <random>
#include <sstream>
#include <ranges>
#include <algorithm>
#include <unbound.h>
#include <unicode/uidna.h>
#include <event2/util.h>
#include <event2/event.h>
#include "covent/sockaddr-cast.h"
#include "covent/dns.h"
#include "covent/future.h"
#include "covent/covent.h"


using namespace covent;
using namespace covent::dns;
using namespace covent::dns::utils;

#ifdef HAVE_ICU2
std::string Utils::toASCII(std::string const &input) {
    if (std::find_if(input.begin(), input.end(), [](const char c) { return c & (1 << 7); }) == input.end())
        return input;
    static UIDNA *idna = 0;
    UErrorCode error = U_ZERO_ERROR;
    if (!idna) {
        idna = uidna_openUTS46(UIDNA_DEFAULT, &error);
    }
    std::string ret;
    ret.resize(1024);
    UIDNAInfo pInfo = UIDNA_INFO_INITIALIZER;
    auto sz = uidna_nameToASCII_UTF8(idna, input.data(), input.size(), const_cast<char *>(ret.data()), 1024, &pInfo,
                                     &error);
    ret.resize(sz);
    return ret;
}
#else
#ifdef HAVE_ICUXX
std::string Utils::toASCII(std::string const &input) {
    if (std::find_if(input.begin(), input.end(), [](const char c) { return c & (1 << 7); }) == input.end())
        return input;
    static UIDNA *idna = 0;
    UErrorCode error = U_ZERO_ERROR;
    if (!idna) {
        idna = uidna_openUTS46(UIDNA_DEFAULT, &error);
    }
    std::string ret;
    ret.resize(1024);
    UIDNAInfo pInfo = UIDNA_INFO_INITIALIZER;
    auto sz = uidna_nameToASCII_UTF8(idna, input.data(), input.size(), const_cast<char *>(ret.data()), 1024, &pInfo,
                                     &error);
    ret.resize(sz);
    return ret;
}
#else

std::string utils::toASCII(std::string const &input) {
    if (std::ranges::find_if(input, [](const char c) { return c & (1 << 7); }) == input.end()) {
        std::string ret = input;
        std::ranges::transform(ret, ret.begin(),
                       [](const char c) { return static_cast<char>(tolower(c)); });
        return ret;
    }
    throw std::runtime_error("IDNA domain but no ICU");
}

#endif
#endif


template<>
uint8_t utils::ntoh<uint8_t>(uint8_t u) {
    return u;
}

template<>
uint16_t utils::ntoh<uint16_t>(uint16_t u) {
    return ntohs(u);
}

template<>
uint32_t utils::ntoh<uint32_t>(uint32_t u) {
    return ntohl(u);
}

std::string covent::dns::utils::read_hostname(std::istringstream & ss) {
    std::string hostname;
    for(std::string label = read_pf_string<uint8_t>(ss); !label.empty(); label = read_pf_string<uint8_t>(ss)) {
        hostname += label;
        hostname += '.';
    }
    if (hostname.empty()) return ".";
    return hostname;
}

rr::SRV rr::SRV::parse(std::string const & s) {
    std::istringstream ss(s);
    rr::SRV rr;
    rr.priority = utils::read_uint<uint16_t>(ss);
    rr.weight = read_uint<uint16_t>(ss);
    rr.port = read_uint<uint16_t>(ss);
    rr.hostname = read_hostname(ss);
    return rr;
}

rr::SVCB rr::SVCB::parse(std::string const & s) {
    std::istringstream ss(s);
    rr::SVCB rr;
    rr.priority = read_uint<uint16_t>(ss);
    rr.hostname = read_hostname(ss);
    long last = -1;
    while(!ss.eof()) {
        uint16_t param;
        try {
            param = read_uint<uint16_t>(ss);
        } catch (std::runtime_error & e) {
            break;
        }
        if (param <= last) {
            throw std::runtime_error("Duplicate/out of order SvcParam");
        }
        last = param;
        switch(param) {
            case 1: // ALPN
            {
                auto len = read_uint<uint16_t>(ss);
                while (len > 0) {
                    auto alpn = read_pf_string<uint8_t>(ss);
                    if (alpn.length() + 1 > len) {
                        throw std::runtime_error("ALPN value overrun");
                    }
                    len -= alpn.length();
                    len -= 1;
                    rr.alpn.insert(alpn);
                }
            }
            break;

            case 3: // Port
                if (read_uint<uint16_t>(ss) != 2) {
                    throw std::runtime_error("Unexpected length for port");
                }
                rr.port = read_uint<uint16_t>(ss);
                break;

            default:
                rr.params[param] = read_pf_string<uint16_t>(ss);
        }
    }
    return rr;
}

rr::TLSA rr::TLSA::parse(std::string const & s) {
    std::istringstream ss(s);
    rr::TLSA rr;
    rr.certUsage = static_cast<CertUsage>(read_uint<uint8_t>(ss));
    rr.selector = static_cast<Selector>(read_uint<uint8_t>(ss));
    rr.matchType = static_cast<MatchType>(read_uint<uint8_t>(ss));
    std::ostringstream os;
    os << ss.rdbuf();
    rr.matchData = os.str();
    return rr;
}


/*
 * dns resolver functions.
 */

namespace {
    class UBResult {
        /* Quick guard class. */
    public:
        struct ub_result *result;

        explicit UBResult(struct ub_result *r) : result(r) {}
        UBResult(UBResult const &) = delete;
        UBResult(UBResult &&) = delete;
        UBResult & operator=(UBResult const &) = delete;
        UBResult & operator=(UBResult &&) = delete;
        ub_result * operator -> () {
            return result;
        }

        ~UBResult() { ub_resolve_free(result); }
    };
}

namespace {
    void event_callback(evutil_socket_t, short, void *arg) {
        while (ub_poll(static_cast<struct ub_ctx *>(arg))) {
            ub_process(static_cast<struct ub_ctx *>(arg));
        }
    }
}

Resolver::Resolver(bool dnssec_required, bool tls, std::optional<std::string> ta_file) : m_dnssec_required(dnssec_required) {
    m_ub_ctx = ub_ctx_create();
    ub_ctx_set_tls(m_ub_ctx, tls ? 1 : 0);
    // TODO : ub_ctx_set_fwd(m_ub_ctx, "");
    if (int retval = ub_ctx_async(m_ub_ctx, 1); retval != 0) {
        throw std::runtime_error(ub_strerror(retval));
    }
    if (int retval = ub_ctx_resolvconf(m_ub_ctx, nullptr); retval != 0) {
        throw std::runtime_error(ub_strerror(retval));
    }
    if (int retval = ub_ctx_hosts(m_ub_ctx, nullptr); retval != 0) {
        throw std::runtime_error(ub_strerror(retval));
    }
    if (ta_file.has_value()) {
        if (int retval = ub_ctx_add_ta_file(m_ub_ctx, ta_file.value().c_str()); retval != 0) {
            throw std::runtime_error(ub_strerror(retval));
        }
    }
    covent::Loop &loop = covent::Loop::thread_loop();
    m_ub_event = event_new(loop.event_base(), ub_fd(m_ub_ctx), EV_READ|EV_PERSIST, event_callback, m_ub_ctx);
    event_add(m_ub_event, nullptr);
}

namespace {
    void callback(void * x, int err, ub_result * res) {
        auto * fut = reinterpret_cast<covent::future<ub_result *> *>(x);
        if (err) {
            try {
                throw std::runtime_error(ub_strerror(err));
            } catch (...) {
                fut->exception(std::current_exception());
            }
        }
        fut->resolve(res);
    }

}

void Resolver::cancel(int async_id) {
    ub_cancel(m_ub_ctx, async_id);
}

Resolver::query Resolver::resolve_async(std::string const &record, int rrtype, covent::future<ub_result *> * fut) {
    int retval;
    int async_id;
    if ((retval = ub_resolve_async(m_ub_ctx, const_cast<char *>(record.c_str()), rrtype, 1,
                                   static_cast<void *>(fut), callback, &async_id)) < //NOSONAR(cpp:S3630)
        0) {
        throw std::runtime_error(std::string("While resolving ") + record + ": " + ub_strerror(retval));
    }
    return query(*this, async_id);
}

namespace {
    template<typename Ans>
    Ans init_answer(ub_result * result, bool dnssec_required) {
        Ans answer;
        if (!result->havedata) {
            answer.error = "No records present";
        } else if (result->bogus) {
            answer.error = std::string("Bogus: ") + result->why_bogus;
        } else if (!result->secure && dnssec_required) {
            answer.error = "DNSSEC required but unsigned";
        } else {
            answer.domain = result->qname;
            answer.dnssec = !!result->secure;
        }
        return answer;
    }
}

covent::instant_task<answers::Address> covent::dns::Resolver::address_v4(std::string const &ihostname) {
    std::string hostname = toASCII(ihostname);
    covent::future<ub_result *> fut;
    auto query = resolve_async(hostname, 1, &fut);
    UBResult result{co_await fut};
    auto address = init_answer<answers::Address>(result.result, m_dnssec_required);
    if (address.error.empty()) {
        for (int i = 0; result->data[i]; ++i) {
            auto it = address.addr.emplace(address.addr.begin());
            auto sin = sockaddr_cast<AF_INET>(std::to_address(it));
            sin->sin_family = AF_INET;
            memcpy(&sin->sin_addr.s_addr, result->data[i], sizeof(decltype(sin->sin_addr.s_addr)));
        }
    }
    co_return address;
}

covent::instant_task<answers::Address> covent::dns::Resolver::address_v6(std::string const &ihostname) {
    std::string hostname = toASCII(ihostname);
    covent::future<ub_result *> fut;
    auto query = resolve_async(hostname, 28, &fut);
    UBResult result{co_await fut};
    auto address = init_answer<answers::Address>(result.result, m_dnssec_required);
    if (address.error.empty()) {
        for (int i = 0; result->data[i]; ++i) {
            auto it = address.addr.emplace(address.addr.begin());
            auto sin = sockaddr_cast<AF_INET6>(std::to_address(it));
            sin->sin6_family = AF_INET6;
            memcpy(sin->sin6_addr.s6_addr, result->data[i], 16);
        }
    }
    co_return address;
}

covent::instant_task<answers::SRV> covent::dns::Resolver::srv(std::string const & service, std::string const &base_domain) {
    std::string domain = toASCII("_" + service + "._tcp." + base_domain + ".");
    covent::future<ub_result *> fut;
    auto query = resolve_async(domain, 33, &fut);
    UBResult result{co_await fut};
    auto srv = init_answer<answers::SRV>(result.result, m_dnssec_required);
    if (srv.error.empty()) {
        for (int i = 0; result->data[i]; ++i) {
            srv.rrs.push_back(rr::SRV::parse(std::string(result->data[i], result->len[i])));
        }
    }
    co_return srv;
}

covent::instant_task<answers::SVCB> Resolver::svcb(std::string const & service, std::string const &base_domain) {
    std::string domain = toASCII("_" + service + "." + base_domain + ".");
    covent::future<ub_result *> fut;
    auto query = resolve_async(domain, 65, &fut);
    UBResult result{co_await fut};
    auto svcb = init_answer<answers::SVCB>(result.result, m_dnssec_required);
    if (svcb.error.empty()) {
        for (int i = 0; result->data[i]; ++i) {
            svcb.rrs.push_back(rr::SVCB::parse(std::string(result->data[i], result->len[i])));
        }
    }
    co_return svcb;
}

covent::instant_task<answers::TLSA> covent::dns::Resolver::tlsa(unsigned short port, std::string const &base_domain) {
    std::ostringstream out;
    out << "_" << port << "._tcp." << base_domain;
    std::string domain = toASCII(out.str());
    covent::future<ub_result *> fut;
    auto query = resolve_async(domain, 52, &fut);
    UBResult result{co_await fut};
    auto tlsa = init_answer<answers::TLSA>(result.result, m_dnssec_required);
    if (tlsa.error.empty()) {
        for (int i = 0; result->data[i]; ++i) {
            tlsa.rrs.push_back(rr::TLSA::parse(result->data[i]));
        }
    }
    co_return tlsa;
}

covent::dns::Resolver::~Resolver() {
    if (m_ub_ctx) {
        for (auto async_id: m_queries) {
            ub_cancel(m_ub_ctx, async_id);
        }
        if (m_ub_event) {
            event_del(m_ub_event);
            event_free(m_ub_event);
        }
        ub_ctx_delete(m_ub_ctx);
    }
}
