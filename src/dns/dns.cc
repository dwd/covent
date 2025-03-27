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

rr::SRV rr::SRV::parse(std::string const & s, std::string const & service) {
    std::istringstream ss(s);
    rr::SRV rr;
    rr.priority = utils::read_uint<uint16_t>(ss);
    rr.weight = read_uint<uint16_t>(ss);
    rr.port = read_uint<uint16_t>(ss);
    rr.hostname = read_hostname(ss);
    rr.service = service;
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

    std::set<Resolver *> & resolvers() {
        static std::set<Resolver *> res;
        return res;
    }


    void event_callback_anon(evutil_socket_t, short, void *arg) {
        Resolver * self = static_cast<Resolver *>(arg);
        if (resolvers().contains(self)) {
            self->event_callback();
        }
    }
}

void Resolver::event_callback() {
    auto hold = m_ub_ctx;
    while (ub_poll(hold->get())) {
        ub_process(hold->get());
    }
}

Resolver::Resolver(bool dnssec_required, bool tls, std::optional<std::string> ta_file) : m_dnssec_required(dnssec_required) {
    setup_unbound(tls, ta_file);
}

Resolver::Resolver(bool tls) : m_dnssec_required(false) {
    setup_unbound(tls, {});
}

void Resolver::setup_unbound(bool tls, std::optional<std::string> const & ta_file) {
    m_ub_ctx = std::make_shared<ctx_holder>(ub_ctx_create());
    ub_ctx_set_tls(m_ub_ctx->get(), tls ? 1 : 0);
    // TODO : ub_ctx_set_fwd(m_ub_ctx->get(), "");
    if (int retval = ub_ctx_async(m_ub_ctx->get(), 1); retval != 0) {
        throw std::runtime_error(ub_strerror(retval));
    }
    if (int retval = ub_ctx_resolvconf(m_ub_ctx->get(), nullptr); retval != 0) {
        throw std::runtime_error(ub_strerror(retval));
    }
    if (int retval = ub_ctx_hosts(m_ub_ctx->get(), nullptr); retval != 0) {
        throw std::runtime_error(ub_strerror(retval));
    }
    if (ta_file.has_value()) {
        if (int retval = ub_ctx_add_ta_file(m_ub_ctx->get(), ta_file.value().c_str()); retval != 0) {
            throw std::runtime_error(ub_strerror(retval));
        }
    }
    // ub_ctx_data_add(m_ub_ctx->get(), /* zone-ish entry here */);
    covent::Loop &loop = covent::Loop::thread_loop();
    m_ub_event = event_new(loop.event_base(), ub_fd(m_ub_ctx->get()), EV_READ|EV_PERSIST, event_callback_anon, this);
    resolvers().insert(this);
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
    ub_cancel(m_ub_ctx->get(), async_id);
}

Resolver::query Resolver::resolve_async(std::string const &record, int rrtype, covent::future<ub_result *> * fut) {
    int retval;
    int async_id;
    if ((retval = ub_resolve_async(m_ub_ctx->get(), const_cast<char *>(record.c_str()), rrtype, 1,
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

covent::task<answers::Address> Resolver::address_v4(std::string ihostname) {
    if (m_a4) co_return *m_a4;
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

void Resolver::inject(answers::Address const & address) {
    bool has4 = false;
    for (auto const & a : address.addr) {
        if (a.ss_family == AF_INET) {
            has4 = true;
            break;
        }
    }
    m_a4 = std::make_unique<answers::Address>(address);
    if (has4) {
        std::erase_if(m_a4->addr, [](auto & x) { return x.ss_family != AF_INET; });
    } else {
        m_a4->addr.clear();
        m_a4->error = "None provided in inject";
    }
    bool has6 = false;
    for (auto const & a : address.addr) {
        if (a.ss_family == AF_INET6) {
            has6 = true;
            break;
        }
    }
    m_a6 = std::make_unique<answers::Address>(address);
    if (has6) {
        std::erase_if(m_a6->addr, [](auto & x) { return x.ss_family != AF_INET6; });
    } else {
        m_a6->addr.clear();
        m_a6->error = "None provided in inject";
    }
}

covent::task<answers::Address> Resolver::address_v6(std::string ihostname) {
    if (m_a6) co_return *m_a6;
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

covent::task<answers::SRV> Resolver::srv(std::string service, std::string base_domain) {
    if (const auto it{m_srv.find(service)}; it != m_srv.end()) {
        co_return *it->second;
    }
    std::string domain = toASCII("_" + service + "._tcp." + base_domain + ".");
    covent::future<ub_result *> fut;
    auto query = resolve_async(domain, 33, &fut);
    UBResult result{co_await fut};
    auto srv = init_answer<answers::SRV>(result.result, m_dnssec_required);
    if (srv.error.empty()) {
        for (int i = 0; result->data[i]; ++i) {
            srv.rrs.push_back(rr::SRV::parse(std::string(result->data[i], result->len[i]), service));
        }
    }
    co_return srv;
}

void Resolver::inject(answers::SRV const &srv) {
    for (auto const & rr : srv.rrs) {
        auto it = m_srv.find(rr.service);
        if (it == m_srv.end()) {
            auto [new_it, ok] = m_srv.emplace(rr.service, std::make_unique<answers::SRV>(srv));
            if (ok) {
                it = new_it;
                it->second->rrs.clear();
            } else {
                continue;
            }
        }
        it->second->rrs.push_back(rr);
    }
}

covent::task<answers::SVCB> Resolver::svcb(std::string service, std::string base_domain) {
    if (m_svcb) {
        co_return *m_svcb;
    }
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

void Resolver::inject(answers::SVCB const & svcb) {
    m_svcb = std::make_unique<answers::SVCB>(svcb);
}

covent::task<answers::TLSA> Resolver::tlsa(unsigned short port, std::string base_domain) {
    if (m_tlsa) co_return *m_tlsa;
    std::ostringstream out;
    out << "_" << port << "._tcp." << base_domain;
    std::string domain = toASCII(out.str());
    covent::future<ub_result *> fut;
    auto query = resolve_async(domain, 52, &fut);
    UBResult result{co_await fut};
    auto tlsa = init_answer<answers::TLSA>(result.result, m_dnssec_required);
    if (tlsa.error.empty()) {
        for (int i = 0; result->data[i]; ++i) {
            tlsa.rrs.push_back(rr::TLSA::parse(std::string(result->data[i], result->len[i])));
        }
    }
    co_return tlsa;
}

void Resolver::inject(answers::TLSA const & tlsa) {
    m_tlsa = std::make_unique<answers::TLSA>(tlsa);
}

void Resolver::add_data(std::string const & zone_record) {
    ub_ctx_data_add(m_ub_ctx->get(), zone_record.c_str());
}

Resolver::~Resolver() {
    resolvers().erase(this);
    if (m_ub_ctx) {
        for (auto async_id: m_queries) {
            ub_cancel(m_ub_ctx->get(), async_id);
        }
        if (m_ub_event) {
            event_del(m_ub_event);
            event_free(m_ub_event);
        }
    }
}

Resolver::ctx_holder::~ctx_holder() {
    ub_ctx_delete(ctx);
}
