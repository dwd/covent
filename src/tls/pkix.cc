//
// Created by dwd on 9/17/24.
//

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/decoder.h>
#include <openssl/rand.h>
#include <openssl/x509v3.h>
#include <fstream>
#include <list>
#include <memory>
#include <functional>
#include <algorithm>
#include "covent/pkix.h"

#include <covent/app.h>
#include <covent/service.h>

#include "covent/covent.h"
#include "covent/crl-cache.h"

using namespace covent::pkix;

namespace {
    std::string const dh_str_4096 = R"(-----BEGIN DH PARAMETERS-----
MIICCAKCAgEAk9O+tKPjzXUxBEnRO6ktnsQh+oMxDS/3QDmKh9cEaoGx81gzH5Xl
Iiu5GZqKND90QOlkwXcyjGzXdIxU8QEfSvo6zsIkyhPuu4ZkOuy8TMvG34Jgv19k
Pbz6n5u9HhsiasLaLd8Cf3Dm/uaA+19PjLA8hlVoj+Tqvmk/3z1tDIRGkynLUOxF
83DEwmocOHWD2y1FBlDL60Noo5yKGf9zyDnTRN6uOTO7+LZW1bglyQ2GrzL291ac
WpxP2gcmdEbEmrT2jCaJALDgtU3cWmW19Nvy5sgtFEZ9l4dWpyq7sRncUBHwo8Z+
5x/WJKXgZdzo68YK5CtbmD57Zn1iy1eUAB9kxR8JHDTPOPg6LxfK3uecWNyS5T/I
xSSB+jvqf39ayA+mcQm9oKH+VY5w3dd7B+0oiFemP4li70Ym9K6uKpStbYUFmUbg
lUojTn/2/wIbq7VFylqlc659VfKY0yQ23eOySO2u6MhpxCsexG5i6NbqfHP+06i6
sIuZsWjDoaOQo1e2n2zeTwYt1qeyrt1ChVy3eXHN0BHhqF5ltez0r0IoZ/AwQ3rz
Zoz/Ee1FLNFOLdghBXTNGORdbSC3O8UEoq13vwkgf3v0sfewhzdTzXIhvLCWQlNH
UcahR3Wj0J6PZ6XVMjKSRX2w97tXDyGfaUXRJnPNrOzyJIo/gE9J9K8CAQI=
-----END DH PARAMETERS-----
)";
    std::string const dh_str_3072 = R"(-----BEGIN DH PARAMETERS-----
MIIBiAKCAYEA61Pa5ngNNeU3sCgh30WrB7ktstxHs/i7haokrhSsQGK4+Ha4w/UI
KnQXT4WNj1tJTUW9rCHuW6gYNCpIzqVi32a0iBmE7fVQvM+5lpFbB/5xITJZTmUu
4Z9RGJRw8klgS8G3qwHc1hkPxdAtP2nfvpc7W/iOncz9ayQ05pn9cKSBFWTSoM9d
8oBD7zQ/35lovoFx2zaO8p2FmYxH3SS+qziQHU+sALN1Z90vV1/eLBUnlfLFEhqU
u6K5klqSM1Bi7gH5xhzD0b+NMm4xjojIUXwpblmim4yAbfmS/W1tiGndzO/4W58X
StnV8hzHqonVgvbkskfxaj9jncu5oLpRdv87eEE6OFtjQatLI5qg8GuHqsYGgRRS
4fyBjkJXxzK+Ltnssemu8D9T2KbagsKAwZ/9clBhsCeCD6ex3dkRwcYNv6+7BCNK
ZCg9+ojbvTMqBNWm2vblt/mRp7DUg9jSDPldwp6DwKmQV9XFV8NnSjJFlzoXFo4x
xK4ykAz8PqxfAgEC
-----END DH PARAMETERS-----
)";
    std::string const dh_str_2236 = R"(-----BEGIN DH PARAMETERS-----
MIIBHwKCARgMe1n9pTQMdQp/0kZfq6qo7s1aBBJE1fm5324517qc5p85jehRW3NQ
Zo8L47A80WopBsRxHWLentDfjofoVZIsj2rkYcAPWtXs6S1cY0FpzKE6NJ1R+uEw
n6oodtKjncmXbLdcud/sw0GHeorYX17OfpGu5skqJFQGDj20FIpxmDvZQBaN6E4H
cbvfxfZw5kQjYFQTRr4Lo19veOagChSS8xPlA6LpnRkAd0GJBwUpBozXuaZRK78v
9oluK6tLNcA9XdXwQWj77wr9AzCIvmqTzjRRXukVACFVNyBOhBrCLEN4jIlfxMpY
BckUuWW9ryzNRkdSpR9BOLeYnBbqyTR+zrI7ZQHBHNcCR+QqguhxKopRFibOUGIH
AgEC
-----END DH PARAMETERS-----
)";
    std::string const dh_str_2048 = R"(-----BEGIN DH PARAMETERS-----
MIIBCAKCAQEA/cHG04YT8IdL4GaMId//cf+M1YhI3wLqWa3Ad2rc2HlObKPKSBSR
LwiUy62WdhcBJsSmhFKCPpQ3ma7YpbTBKFLWJ0SdaspipGdYIk8TsgN5S9WL7LxA
HsCdPC8SnjC8k7G35vulwKVOdfhOeyRGjEsvuz2JohlIFQUOLXuGeuTSZjRVd4md
1GEYuuYCKTSJvnKDZ2PCen9Kn5726x9ZP/kDuFMopqH5uTfTbtimZ6Bhaxjnft+0
EAhurLOF+ETqJav393WOQH5lwm/Eorr6lfl1kwQhpNUEAsLWYz0y46e7CO31tzIf
TjuAW7Ho3gCaeg7QiGpGiwr+2Yt4j8hl7wIBAg==
-----END DH PARAMETERS-----
)";
    std::string const dh_str_1024 = R"(-----BEGIN DH PARAMETERS-----
MIGHAoGBAILtTtZQdevX4/JhgmxuMRRTEQlFtp491NLc7nkykFrGIOIhnLhQEXaj
ZPvubjYBNqfMEkPAefyNEwVrIL9Wg9+K4D130Lqt//qLUJlWT60+LlbdLUdBmeMh
EjhZjvPJOKqTisDI6g9A9ak87cfIh26eYj+vm5JOnjYltmaZ6U83AgEC
-----END DH PARAMETERS-----
)";
    EVP_PKEY * get_builtin_dh(int keylength) {
        std::string const * dh_str = &dh_str_2236;
        int actual_keylen = 2236;
        static std::map<int,EVP_PKEY *> s_cache;
        if (keylength == 0) {
            // Defaults as above.
        } else if (keylength < 2048) {
            dh_str = &dh_str_1024;
            actual_keylen = 1024;
        } else if (keylength < 2236) {
            dh_str = &dh_str_2048;
            actual_keylen = 2048;
        } else if (keylength < 3072) {
            dh_str = &dh_str_2236;
            actual_keylen = 2236;
        } else if (keylength < 4096) {
            dh_str = &dh_str_3072;
            actual_keylen = 3072;
        } else if (keylength == 4096) {
            dh_str = &dh_str_4096;
            actual_keylen = 4096;
        } else {
            throw dhparam_error("Don't have a packages DH key that size, sorry.");
        }
        if (s_cache.contains(actual_keylen)) {
            return s_cache[actual_keylen];
        }
        EVP_PKEY * evp = nullptr;
        auto * dctx = OSSL_DECODER_CTX_new_for_pkey(&evp, "PEM", nullptr, "DH", OSSL_KEYMGMT_SELECT_ALL_PARAMETERS, nullptr, nullptr);
        std::vector<unsigned char> tmp(dh_str->begin(), dh_str->end());
        const auto * keydata = tmp.data();
        auto keylen = tmp.size();
        if(OSSL_DECODER_from_data(dctx, &keydata, &keylen)) {
            EVP_PKEY_up_ref(evp);
            s_cache[actual_keylen] = evp;
            return evp;
        } else {
            throw dhparam_error("Decoding of internal DH params failed");
        }
    }
    EVP_PKEY * get_file_dh(std::string const & filename) {
        EVP_PKEY * evp = nullptr;
        static std::map<std::string,EVP_PKEY *,std::less<>> s_cache;
        auto * dctx = OSSL_DECODER_CTX_new_for_pkey(&evp, "PEM", nullptr, "DH", OSSL_KEYMGMT_SELECT_ALL_PARAMETERS, nullptr, nullptr);
        auto fclose_wrapper = [](FILE * fp) {
            if (fp) fclose(fp);
        };
        std::unique_ptr<FILE, decltype(fclose_wrapper)> fp{fopen(filename.c_str(), "rb"), fclose_wrapper};
        if(OSSL_DECODER_from_fp(dctx, fp.get())) {
            EVP_PKEY_up_ref(evp);
            s_cache[filename] = evp;
            return evp;
        } else {
            throw dhparam_error("Decoding of external DH params failed");
        }
    }

    int string_to_tls(std::string const &tls_version, int def) {
        auto version_string = tls_version;
        int version = def;
        std::ranges::transform(version_string, version_string.begin(), [](unsigned char c) {
            return static_cast<unsigned char>(std::tolower(c));
        });
        std::erase(version_string, 'v');
        std::erase(version_string, '.');
        if (version_string == "ssl2") {
            version = SSL2_VERSION;
        } else if (version_string == "ssl3") {
            version = SSL3_VERSION;
        } else if (version_string == "tls1" || version_string == "tls10") {
            version = TLS1_VERSION;
        } else if (version_string == "tls11") {
            version = TLS1_1_VERSION;
        } else if (version_string == "tls12") {
            version = TLS1_2_VERSION;
        } else if (version_string == "tls13") {
            version = TLS1_3_VERSION;
        }
        return version;
    }

    constexpr const char * tls_version_to_string(int ver) {
        switch (ver) {
            case SSL2_VERSION:
                return "SSLv2";
            case SSL3_VERSION:
                return "SSLv3";
            case TLS1_VERSION:
                return "TLSv1.0";
            case TLS1_1_VERSION:
                return "TLSv1.1";
            case TLS1_2_VERSION:
                return "TLSv1.2";
            case TLS1_3_VERSION:
                return "TLSv1.3";
            default:
                return nullptr;
        }
    }


    int service_exdata_index() {
        static int index = -1;
        if (index == -1) {
            index = SSL_CTX_get_ex_new_index(0, nullptr, nullptr, nullptr, nullptr);
        }
        return index;
    }


    int ssl_servername_cb(SSL *ssl, int *, void *) {
        const char *servername = SSL_get_servername(ssl, TLSEXT_NAMETYPE_host_name);
        if (!servername) return SSL_TLSEXT_ERR_OK;
        SSL_CTX const * const old_ctx = SSL_get_SSL_CTX(ssl);
        auto & service = *static_cast<covent::Service *>(SSL_CTX_get_ex_data(old_ctx, service_exdata_index()));
        SSL_CTX  * new_ctx = nullptr;
        std::string domain_name = servername; // Jid(servername).domain();
        auto & entry = service.entry(domain_name);
        auto & tls_context = entry.tls_context();
        new_ctx = tls_context.context();
        if (new_ctx != old_ctx) SSL_set_SSL_CTX(ssl, new_ctx);
        return SSL_TLSEXT_ERR_OK;
    }

    int verify_callback_cb(int preverify_ok, X509_STORE_CTX *st) {
        if (!preverify_ok) {
            const int name_sz = 256;
            std::string cert_name;
            cert_name.resize(name_sz);
            X509_NAME_oneline(X509_get_subject_name(X509_STORE_CTX_get_current_cert(st)),
                              cert_name.data(), name_sz);
            cert_name.resize(cert_name.find('\0'));
//            Config::config().logger().info("Cert failed basic verification: {}", cert_name);
//            Config::config().logger().info("Error is {}", X509_verify_cert_error_string(X509_STORE_CTX_get_error(st)));
        } else {
            const int name_sz = 256;
            std::string cert_name;
            cert_name.resize(name_sz);
            X509_NAME_oneline(X509_get_subject_name(X509_STORE_CTX_get_current_cert(st)),
                              cert_name.data(), name_sz);
            cert_name.resize(cert_name.find('\0'));
//            Config::config().logger().debug("Cert passed basic verification: {}", cert_name);
//            if (Config::config().fetch_pkix_status()) {
//                auto cert = X509_STORE_CTX_get_current_cert(st);
//                std::unique_ptr<STACK_OF(DIST_POINT), std::function<void(STACK_OF(DIST_POINT) *)>> crldp_ptr{
//                        (STACK_OF(DIST_POINT) *) X509_get_ext_d2i(cert, NID_crl_distribution_points, nullptr, nullptr),
//                        [](STACK_OF(DIST_POINT) *crldp) { sk_DIST_POINT_pop_free(crldp, DIST_POINT_free); }};
//                auto crldp = crldp_ptr.get();
//                if (crldp) {
//                    for (int i = 0; i != sk_DIST_POINT_num(crldp); ++i) {
//                        auto const *const dp = sk_DIST_POINT_value(crldp, i);
//                        if (dp->distpoint->type == 0) { // Full Name
//                            auto names = dp->distpoint->name.fullname;
//                            for (int ii = 0; ii != sk_GENERAL_NAME_num(names); ++ii) {
//                                auto const *const name = sk_GENERAL_NAME_value(names, ii);
//                                if (name->type == GEN_URI) {
//                                    ASN1_IA5STRING *uri = name->d.uniformResourceIdentifier;
//                                    std::string uristr{reinterpret_cast<char *>(uri->data),
//                                                       static_cast<std::size_t>(uri->length)};
//                                    Config::config().logger().info("Prefetching CRL for {} - {}", cert_name, uristr);
//                                    auto coro = CrlCache::crl(uristr);
//                                    coro.start(); // But then we'd destroy it instantly, so maybe not?
//                                }
//                            }
//                        }
//                    }
//                }
//            }
        }
        return 1;
    }
    int reverify_callback(int preverify_ok, X509_STORE_CTX *st) {
        std::array<char, 256> buffer{};
        std::string cert_name{"<no cert name>"};
        if (auto cert = X509_STORE_CTX_get_current_cert(st)) {
            X509_NAME_oneline(X509_get_subject_name(cert), buffer.data(), buffer.size());
            cert_name = buffer.data();
        }
        auto depth = X509_STORE_CTX_get_error_depth(st);
        //        if (preverify_ok) {
        //            Config::config().logger().info("Cert {} passed reverification: {}", depth, cert_name);
        //        } else {
        //            Config::config().logger().info("Cert {} failed reverification: {}", depth, cert_name);
        //            Config::config().logger().info("Error is {}", X509_verify_cert_error_string(X509_STORE_CTX_get_error(st)));
        //        }
        return preverify_ok;
    }

}

PKIXIdentity::PKIXIdentity(std::string const & cert_chain, std::string const & pkey) : m_cert_chain_file(cert_chain), m_pkey_file(pkey) {
}

void PKIXIdentity::apply(SSL_CTX * ssl_ctx) const {
    spdlog::default_logger()->info("Loading identity for cert {}", m_cert_chain_file);
    if (SSL_CTX_use_certificate_chain_file(ssl_ctx, m_cert_chain_file.c_str()) != 1) {
        for (unsigned long e = ERR_get_error(); e != 0; e = ERR_get_error()) {
            //            Config::config().logger().error("OpenSSL Error (chain): {}", ERR_reason_error_string(e));
        }
        throw pkix_identity_load_error("Couldn't load chain file: " + m_cert_chain_file);
    }
    if (SSL_CTX_use_PrivateKey_file(ssl_ctx, m_pkey_file.c_str(), SSL_FILETYPE_PEM) != 1) {
        for (unsigned long e = ERR_get_error(); e != 0; e = ERR_get_error()) {
            //            Config::config().logger().error("OpenSSL Error (pkey): {}", ERR_reason_error_string(e));
        }
        throw pkix_identity_load_error("Couldn't load keyfile: " + m_pkey_file);
    }
    if (SSL_CTX_check_private_key(ssl_ctx) != 1) {
        for (unsigned long e = ERR_get_error(); e != 0; e = ERR_get_error()) {
            //            Config::config().logger().error("OpenSSL Error (check): {}", ERR_reason_error_string(e));
        }
        throw pkix_config_error("Private key mismatch");
    }
    spdlog::default_logger()->info("Loaded identity for cert {}", m_cert_chain_file);
}

PKIXValidator::PKIXValidator(Service & service, bool crls, bool use_system_trust) : m_service(service), m_enabled(true), m_crls(crls), m_system_trust(use_system_trust) {
    m_log = Application::application().logger("PKIXValidator");
}


covent::task<void> PKIXValidator::fetch_crls(const SSL *ssl, X509 *cert) {
    STACK_OF(X509) *chain = SSL_get_peer_cert_chain(ssl);
    const SSL_CTX *ctx = SSL_get_SSL_CTX(ssl);
    X509_STORE *store = SSL_CTX_get_cert_store(ctx);
    X509_STORE_CTX *st = X509_STORE_CTX_new();
    struct destroy_store {
        X509_STORE_CTX * st;
        ~destroy_store() {
            if (st) X509_STORE_CTX_free(st);
        }
    } store_keeper = {st};
    X509_STORE_CTX_init(st, store, cert, chain);
    X509_verify_cert(st);
    STACK_OF(X509) *verified = X509_STORE_CTX_get0_chain(st);
    std::set<std::string, std::less<>> all_crls;
    for (int certnum = 0; certnum != sk_X509_num(verified); ++certnum) {
        auto current_cert = sk_X509_value(verified, certnum);
        std::unique_ptr<STACK_OF(DIST_POINT), std::function<void(STACK_OF(DIST_POINT) *)>> crldp_ptr{
                (STACK_OF(DIST_POINT) *) X509_get_ext_d2i(current_cert, NID_crl_distribution_points, nullptr, nullptr),
                [](STACK_OF(DIST_POINT) *crldp) { sk_DIST_POINT_pop_free(crldp, DIST_POINT_free); }};
        if (crldp_ptr) {
            auto crldp = crldp_ptr.get();
            for (int i = 0; i != sk_DIST_POINT_num(crldp); ++i) {
                const auto *dp = sk_DIST_POINT_value(crldp, i);
                if (dp->distpoint->type == 0) { // Full Name
                    auto names = dp->distpoint->name.fullname;
                    for (int ii = 0; ii != sk_GENERAL_NAME_num(names); ++ii) {
                        const auto *name = sk_GENERAL_NAME_value(names, ii);
                        if (name->type == GEN_URI) {
                            const auto *uri = name->d.uniformResourceIdentifier;
                            std::string uristr{reinterpret_cast<char *>(uri->data),
                                               static_cast<std::size_t>(uri->length)};
                            m_log->info("verify_tls: Fetching CRL - {}", uristr);
                            all_crls.insert(uristr);
                        }
                    }
                }
            }
        }
    }
    // Now we wait for them all. Order doesn't matter - we'll get new copies
    // in the rare case we happen to cross an expiry boundary, but that's
    // no biggie.
    for (auto & uri : all_crls) {
        auto [uristr, code, crl] = co_await CrlCache::crl(uri);
        m_log->info("verify_tls: Fetched CRL - {}, with code {}", uri, code);
        if (!crl) continue;
        if (!X509_STORE_add_crl(store, crl)) {
            // Erm. Whoops? Probably doesn't matter.
            ERR_clear_error();
        }
    }
    co_return;
}
/**
 * This is a fairly massive coroutine, but I've kept it this way because it's
 * difficult to break apart. Indeed, I pulled it together out of two major callback
 * loops which were pretty nasty in complexity terms.
 *
 * @param stream
 * @param route
 * @return true if TLS verified correctly.
 */
covent::task<bool> PKIXValidator::verify_tls(SSL * ssl, std::string remote_domain) {
    if (!m_enabled) co_return true;
    if (!ssl) co_return false; // No TLS.
    auto *cert = SSL_get_peer_certificate(ssl);
    if (!cert) {
        m_log->info("verify_tls: No cert, so no auth");
        co_return false;
    }
    if (X509_V_OK != SSL_get_verify_result(ssl)) {
        //m_log.info("verify_tls: Cert failed verification but rechecking anyway.");
    } // TLS failed basic verification.
    //m_log.debug("verify_tls: [Re]verifying TLS for {}", remote_domain);
    auto *chain = SSL_get_peer_cert_chain(ssl);
    const auto *ctx = SSL_get_SSL_CTX(ssl);
    X509_STORE *free_store = nullptr;
    auto *store = SSL_CTX_get_cert_store(ctx);
    auto *vpm = X509_VERIFY_PARAM_new();
    if (m_crls) {
        co_await fetch_crls(ssl, cert);
        X509_VERIFY_PARAM_set_flags(vpm, X509_V_FLAG_CRL_CHECK_ALL);
    }
    X509_VERIFY_PARAM_set1_host(vpm, remote_domain.c_str(), remote_domain.size());
    // Add RFC 6125 additional names.
    auto & entry = m_service.entry(remote_domain);
    auto gathered = co_await entry.discovery(remote_domain);
    for (auto const &host : gathered.gathered_hosts) {
        m_log->debug("Adding gathered hostname {}", host);
        X509_VERIFY_PARAM_add1_host(vpm, host.c_str(), host.size());
    }
    auto *st = X509_STORE_CTX_new();
    if (!m_trust_blobs.empty()) {
        store = free_store = X509_STORE_new();
        for (auto * ta : m_trust_blobs) {
            X509_STORE_add_cert(store, ta);
        }
    }
    X509_STORE_CTX_set0_param(st, vpm); // Hands ownership to st.
    // Fun fact: We can only add these to SSL_DANE via the connection.
    for (auto const & rr : gathered.gathered_tlsa) {
        // m_log->debug("Adding TLSA {} / {} / {} with {} bytes of match data", rr.certUsage, rr.selector, rr.matchType, rr.matchData.length());
        if (0 == SSL_dane_tlsa_add(ssl,
                                   std::to_underlying(rr.certUsage),
                                   std::to_underlying(rr.selector),
                                   std::to_underlying(rr.matchType),
                                   reinterpret_cast<const unsigned char *>(rr.matchData.data()), rr.matchData.length())) {
            m_log->warn("TLSA record rejected");
        }
    }
    X509_STORE_CTX_init(st, store, cert, chain);
    X509_STORE_CTX_set0_dane(st, SSL_get0_dane(ssl));
    X509_STORE_CTX_set_verify_cb(st, reverify_callback);
    m_log->info("Reverification for {}", remote_domain);
    bool valid = (X509_verify_cert(st) == 1);
    if (valid) {
        if (gathered.gathered_tlsa.empty()) {
            m_log->info("verify_tls: PKIX verification succeeded");
        } else {
            m_log->info("verify_tls: DANE verification succeeded");
        }
    } else {
        auto error = X509_STORE_CTX_get_error(st);
        auto depth = X509_STORE_CTX_get_error_depth(st);
        std::array<char, 1024> buf = {};
        m_log->warn("verify_tls: Chain failed validation: {} (at depth {})", ERR_error_string(error, buf.data()),
                             depth);
    }
    X509_STORE_CTX_free(st);
    if (free_store) X509_STORE_free(free_store);
    co_return valid;
}

void PKIXValidator::add_trust_anchor(std::string const & filename){
    std::ifstream in(filename);
    std::string value;
    value.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    if (value.starts_with("-----BEGIN")) {
        std::unique_ptr<BIO,decltype(&BIO_free)> bio{BIO_new_mem_buf(value.data(), static_cast<int>(value.size())), &BIO_free};
        auto cert = PEM_read_bio_X509(bio.get(), nullptr, nullptr, nullptr);
        if (!cert) throw pkix_config_error("Invalid PEM certificate");
        m_trust_blobs.push_back(cert);
    } else {
        const unsigned char * ptr = reinterpret_cast<unsigned char *>(value.data());
        auto * cert = d2i_X509(nullptr, &ptr, value.size());
        if (!cert) throw pkix_config_error("Invalid DER certificate");
        m_trust_blobs.push_back(cert);
    }
}

TLSContext::TLSContext(Service & service, bool enabled, bool validation, std::string const & domain) : m_service(service), m_enabled(enabled), m_validation(validation), m_domain(domain) {
    m_log = Application::application().logger("TLSContext '{}'", domain);
    if (m_enabled) {
        m_dhparam = "auto";
        m_cipherlist = "HIGH:!3DES:!eNULL:!aNULL:@STRENGTH"; // Apparently 3DES qualifies for HIGH, but is 112 bits, which the IM Observatory marks down for.
        m_min_version = TLS1_2_VERSION;
        m_max_version = TLS1_3_VERSION;
    }
}

TLSContext::~TLSContext() {
    if (m_ssl_ctx) {
        SSL_CTX_free(m_ssl_ctx);
    }
}


SSL * TLSContext::instantiate(bool connecting, std::string const & domain) {
    if (!m_enabled) return nullptr;
    SSL_CTX *ctx = context();
    SSL *ssl = SSL_new(ctx);
    if (!ssl) throw pkix_error("Failure to initiate TLS, sorry!");
    SSL_dane_enable(ssl, domain.c_str());
    // Cipherlist
    m_log->info("Setting cipherlist {}", m_cipherlist);
    SSL_set_cipher_list(ssl, m_cipherlist.c_str());
    // Min / max TLS versions.
    if (m_min_version != 0) {
        SSL_set_min_proto_version(ssl, m_min_version);
    }
    if (m_max_version != 0) {
        SSL_set_max_proto_version(ssl, m_max_version);
    }
    // DH parameters
    if (m_dhparam == "auto") {
        SSL_set_dh_auto(ssl, 1);
    } else {
        EVP_PKEY * evp = nullptr;
        try {
            int keylen = std::stoi(m_dhparam);
            evp = get_builtin_dh(keylen);
        } catch (std::invalid_argument &) {
//            m_log.debug("Assuming dhparam: '{}' is a filename", m_dhparam);
            // Pass
        }
        if (!evp) {
            evp = get_file_dh(m_dhparam);
        }
        SSL_set0_tmp_dh_pkey(ssl, evp);
    }
    if (!connecting) {
        SSL_set_accept_state(ssl);
    } else { //m_stream.direction() == OUTBOUND
        SSL_set_connect_state(ssl);
        SSL_set_tlsext_host_name(ssl, domain.c_str());
    }
    return ssl;
}

SSL_CTX * TLSContext::context() {
    m_log->info("Getting SSL_CTX");
    if (!m_enabled) return nullptr;
    if (m_ssl_ctx) {
        m_log->info("Using cached SSL_CTX");
        return m_ssl_ctx;
    }
    m_ssl_ctx = SSL_CTX_new(TLS_method());
    SSL_CTX_dane_enable(m_ssl_ctx);
    SSL_CTX_dane_set_flags(m_ssl_ctx, DANE_FLAG_NO_DANE_EE_NAMECHECKS);
    SSL_CTX_set_options(m_ssl_ctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_ALL);
    if (m_validation) {
        SSL_CTX_set_verify(m_ssl_ctx, SSL_VERIFY_PEER, verify_callback_cb);
    } else {
        SSL_CTX_set_verify(m_ssl_ctx, SSL_VERIFY_NONE, verify_callback_cb);
    }
    m_log->info("Adding {} identities", m_identities.size());
    for (auto & identity : m_identities) {
        m_log->info("Adding identity");
        identity->apply(m_ssl_ctx);
    }
    m_log->info("Done");

    SSL_CTX_set_purpose(m_ssl_ctx, X509_PURPOSE_SSL_SERVER);
//    if(SSL_CTX_set_default_verify_paths(m_ssl_ctx) == 0) {
    if(SSL_CTX_load_verify_locations(m_ssl_ctx, nullptr, "/etc/ssl/certs") == 0) {
//        m_log.warn("Loading default verify paths failed:");
        for (unsigned long e = ERR_get_error(); e != 0; e = ERR_get_error()) {
//            m_log.error("OpenSSL Error (default_verify_paths): {}", ERR_reason_error_string(e));
        }
    }
    SSL_CTX_set_ex_data(m_ssl_ctx, service_exdata_index(), &m_service);
    SSL_CTX_set_tlsext_servername_callback(m_ssl_ctx, ssl_servername_cb);
    std::string ctx = "Metre::" + m_domain;
    SSL_CTX_set_session_id_context(m_ssl_ctx, reinterpret_cast<const unsigned char *>(ctx.c_str()),
                                   static_cast<unsigned int>(ctx.size()));
    return m_ssl_ctx;
}

bool TLSContext::enabled() const {
    return m_enabled;
}

void TLSContext::add_identity(std::unique_ptr<PKIXIdentity> &&identity) {
    m_identities.emplace(std::move(identity));
}
