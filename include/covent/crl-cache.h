//
// Created by dwd on 11/18/24.
//

#ifndef COVENT_CRL_CACHE_H
#define COVENT_CRL_CACHE_H

#include <map>
#include <openssl/types.h>
#include <covent/covent.h>

namespace covent::pkix {
    class CrlCache {
    private:
        std::map<std::string, X509_CRL *, std::less<>> m_crl_cache;
    public:
        CrlCache() = default;
        static covent::task<std::tuple<std::string, int, X509_CRL *>> crl(std::string const &uri);
    private:
        static CrlCache & crl_cache();
        covent::task<std::tuple<std::string, int, X509_CRL *>> do_crl(std::string uri);
    };
}

#endif //COVENT_CRL_CACHE_H
