//
// Created by dwd on 30/08/16.
//

#include <covent/covent.h>
#include <covent/http.h>
#include <openssl/ossl_typ.h>
#include <openssl/x509.h>
#include <openssl/err.h>
#include <openssl/ssl.h>

#include <covent/crl-cache.h>

using namespace covent::pkix;

namespace {
    CrlCache * s_crl_cache = nullptr;
}

CrlCache & CrlCache::crl_cache() {
    if (!s_crl_cache) s_crl_cache = new CrlCache();
    return *s_crl_cache;
}

covent::task<std::tuple<std::string, int, X509_CRL *>> CrlCache::crl(std::string const &uri) {
    return CrlCache::crl_cache().do_crl(uri);
}

covent::task<std::tuple<std::string, int, X509_CRL *>> CrlCache::do_crl(std::string const &urix) {
    std::string uri{urix}; // Destructively parsed
    // Step one: Look in cache.
    auto iter = m_crl_cache.find(uri);
    if (iter != m_crl_cache.end() && iter->second) {
        auto data = iter->second;
        auto nextupdate = X509_CRL_get0_nextUpdate(data);
        int day, sec;
        ASN1_TIME_diff(&day, &sec, nullptr, nextupdate);
        if (day > 0) {
            // if nextUpdate is today sometime - refetch.
            // Otherwise just return:
            co_return {uri, 200, data};
        }
    }
    // Step three: Actually issue a new HTTP request:
    covent::http::Request req(covent::http::Request::Method::GET, uri);
    auto response = co_await req();
    int status_code = response.status();
    // METRE_LOG(Log::INFO, "HTTP GET for " << uri << " returned " << status_code);
    if ((status_code / 100) == 2) {
        auto body = response.body();
        auto body_start = reinterpret_cast<const unsigned char *>(body.data());
        X509_CRL *data = d2i_X509_CRL(nullptr, &body_start, body.size());
        if (data) {
            co_return {uri, 200, data};
        } else {
            while (unsigned long ssl_err = ERR_get_error()) {
                std::array<char, 1024> error_buf;
                // METRE_LOG(Metre::Log::DEBUG, " :: " << ERR_error_string(ssl_err, error_buf.data()));
            }
            co_return {uri, 400, nullptr};
        }
    }
    co_return {uri, status_code, nullptr};
}
