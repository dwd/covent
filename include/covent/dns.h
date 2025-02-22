/***

Copyright 2013-2016 Dave Cridland
Copyright 2014-2016 Surevine Ltd

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

***/

#ifndef COVENT_DNS_H
#define COVENT_DNS_H

// #include "fmt-enum.h"
#include "covent/coroutine.h"
#include "covent/future.h"
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <unordered_set>
#include <set>
// #include <spdlog/logger.h>
// For struct sockaddr_storage :
#ifndef COVENT_WIN
#include <netinet/in.h>
#else
#include <winsock2.h>
#endif

// FWD
struct ub_result;
struct ub_ctx;
struct event;

namespace covent::dns {

    namespace utils {
        std::string toASCII(std::string const &);

        template<typename UINT>
        UINT ntoh(UINT);

        template<>
        uint8_t ntoh<uint8_t>(uint8_t u);

        template<>
        uint16_t ntoh<uint16_t>(uint16_t u);

        template<>
        uint32_t ntoh<uint32_t>(uint32_t u);

        template<typename UINT>
        UINT read_uint(std::istringstream &ss) {
            UINT l;
            ss.read(reinterpret_cast<char *>(&l), sizeof(l));
            if (!ss) {
                throw std::runtime_error("End of data in DNS parsing");
            }
            return ntoh(l);
        }

        template<typename UINT>
        std::string read_pf_string(std::istringstream & ss) {
            auto len = read_uint<UINT>(ss);
            if (len == 0) {
                return {};
            }
            std::vector<char> str_data(len);
            ss.read(str_data.data(), len);
            if (!ss) {
                throw std::runtime_error("End of data in DNS parsing");
            }
            return {str_data.begin(), str_data.end()};
        }

        std::string read_hostname(std::istringstream & ss);

        struct ub_ctx * dns_init(std::string const & dns_key_file);
    }

    namespace rr {
        class SRV {
        public:
            std::string hostname;
            unsigned short port = 0;
            unsigned short weight = 0;
            unsigned short priority = 0;
            bool tls = false;

            static SRV parse(std::string const &);
        };

        class SVCB {
        public:
            std::string hostname;
            unsigned short priority = 0;
            unsigned short port = 0;
            std::set<std::string, std::less<>> alpn;
            std::map<long, std::string> params; // These might be binary!
            static SVCB parse(std::string const &);
        };

        class TLSA {
        public:
            enum class CertUsage : uint8_t {
                CAConstraint = 0,
                CertConstraint = 1,
                TrustAnchorAssertion = 2,
                DomainCert = 3
            };
            enum class Selector : uint8_t {
                FullCert = 0,
                SubjectPublicKeyInfo = 1
            };
            enum class MatchType : uint8_t {
                Full = 0,
                Sha256 = 1,
                Sha512 = 2
            };
            CertUsage certUsage;
            Selector selector;
            MatchType matchType;
            std::string matchData;

            static TLSA parse(std::string const &);
        };
    }

    namespace answers {
        class SRV {
        public:
            std::vector<rr::SRV> rrs;
            std::string domain;
            bool dnssec = false;
            std::string error;
        };

        class SVCB {
        public:
            std::vector<rr::SVCB> rrs;
            std::string domain;
            bool dnssec = false;
            std::string error;
        };

        class Address {
        public:
            std::vector<struct sockaddr_storage> addr;
            bool dnssec = false;
            std::string error;
            std::string domain;
        };


        class TLSA {
        public:
            std::string domain;
            std::string error;
            bool dnssec = false;

            std::vector<rr::TLSA> rrs;
        };
    }

    class Resolver {
    public:
        Resolver(bool dnssec_required, bool tls, std::optional<std::string> ta_file);
        explicit Resolver(bool tls);
        Resolver(Resolver const &) = delete;
        Resolver(Resolver &&) = delete;

        ~Resolver();

        void add_data(std::string const & zone_record);

        covent::task<answers::SRV> srv(std::string const & service, std::string const &domain);
        covent::task<answers::SVCB> svcb(std::string const & service, std::string const &domain);
        covent::task<answers::Address> address_v4(std::string const &hostname);
        covent::task<answers::Address> address_v6(std::string const &hostname);

        covent::task<answers::TLSA> tlsa(short unsigned int port, std::string const &hostname);

        void event_callback();

    private:
        void setup_unbound(bool, std::optional<std::string> const &);

        bool m_dnssec_required;
        event * m_ub_event = nullptr;
        struct ctx_holder {
            ub_ctx * ctx;
            ub_ctx * get() const {
                return ctx;
            }
            ~ctx_holder();
        };
        std::shared_ptr<ctx_holder> m_ub_ctx; // Need to keep this around after destruction.
        std::unordered_set<int> m_queries;
        struct query {
            Resolver & resolver;
            int async_id;

            query(Resolver & r, int a) : resolver(r), async_id(a) {
            }
            query(query && o)  noexcept : resolver(o.resolver), async_id(o.async_id) {
                o.async_id = -1;
            }
            query(query const &) = delete;
            ~query() {
                if (async_id != -1) resolver.cancel(async_id);
            }
        };

        query resolve_async(const std::string &record, int rrtype, covent::future<ub_result *> *fut);

        void cancel(int async_id);
    };
}
//
//METRE_ENUM_FORMATTER(Metre::DNS::TlsaRR::CertUsage,
//                     METRE_ENUM_ENTRY_TXT(CertConstraint, "CertConstraint (PKIX-EE)")
//                     METRE_ENUM_ENTRY_TXT(CAConstraint, "CAConstraint (PKIX-CA)")
//                     METRE_ENUM_ENTRY_TXT(DomainCert, "DomainCert (DANE-EE)")
//                     METRE_ENUM_ENTRY_TXT(TrustAnchorAssertion, "TrustAnchorAssertion (DANE-TA)")
// );
//
//METRE_ENUM_FORMATTER(Metre::DNS::TlsaRR::Selector,
//                     METRE_ENUM_ENTRY(SubjectPublicKeyInfo)
//                     METRE_ENUM_ENTRY(FullCert)
//);
//
//METRE_ENUM_FORMATTER(Metre::DNS::TlsaRR::MatchType,
//                     METRE_ENUM_ENTRY_TXT(Sha256, "SHA-256")
//                     METRE_ENUM_ENTRY_TXT(Sha512, "SHA-512")
//                     METRE_ENUM_ENTRY(Full)
//);

#endif
