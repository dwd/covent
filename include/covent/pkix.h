/***

Copyright 2016-2024 Dave Cridland
Copyright 2016 Surevine Ltd

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


#ifndef COVENT_PKIX_H
#define COVENT_PKIX_H

#include <openssl/ossl_typ.h>
#include "covent/dns.h"


namespace covent::pkix {
    class PKIXIdentity {
    public:
        PKIXIdentity() = delete;
        PKIXIdentity(PKIXIdentity const &) = delete;
        explicit PKIXIdentity(std::string const & cert_chain_file, std::string const & pkey_file);

        void operator=(PKIXIdentity const &) = delete;
        void apply(SSL_CTX *) const;

    private:
        std::string m_cert_chain_file;
        std::string m_pkey_file;
        bool m_generate = false;
    };

    class PKIXValidator {
    public:
        PKIXValidator(PKIXValidator const &) = delete;
        explicit PKIXValidator(Service & service, bool crls, bool use_system_trust);
        void add_trust_anchor(std::string const & trust_anchor);
        covent::task<bool> verify_tls(SSL *, std::string);
        covent::task<void> fetch_crls(const SSL *, X509 * cert);

    private:
        Service & m_service;
        bool m_enabled;
        bool m_crls;
        bool m_system_trust;
        std::set<std::string, std::less<>> m_trust_anchors;
        std::vector<X509 *> m_trust_blobs;
    };

    class TLSContext {
    public:
        TLSContext() = delete;
        TLSContext(TLSContext const &) = delete;
        void operator = (TLSContext const &) = delete;
        TLSContext(Service & service, bool enabled, bool validation, std::string const & domain);
        ~TLSContext();
        SSL_CTX* context();
        SSL * instantiate(bool connecting, std::string const & remote_domain);
        bool enabled();
        void add_identity(std::unique_ptr<PKIXIdentity> && identity);

        void enabled(bool e);
        [[nodiscard]] bool validation() const {
            return m_validation;
        }
        void validation(bool v) {
            m_validation = v;
        }
        [[nodiscard]] std::string const & domain() const {
            return m_domain;
        }
        void domain(std::string const & d) {
            m_domain = d;
        }
        [[nodiscard]] std::string const & dhparam() const {
            return m_dhparam;
        }
        void dhparam(std::string const & d) {
            m_dhparam = d;
        }
        [[nodiscard]] std::string const & cipherlist() const {
            return m_cipherlist;
        }
        void cipherlist(std::string const & c) {
            m_cipherlist = c;
        }
        [[nodiscard]] int min_version() const {
            return m_min_version;
        }
        void min_version(int m) {
            m_min_version = m;
        }
        [[nodiscard]] int max_version() const {
            return m_max_version;
        }
        [[nodiscard]] std::string const & max_version_str() const;
        void max_version(int m) {
            m_max_version = m;
        }
        void max_version(std::string const &);
        [[nodiscard]] auto const & identities() const {
            return m_identities;
        }

    private:
        Service & m_service;
        bool m_enabled = true;
        bool m_validation = true;
        std::string m_domain;
        std::string m_dhparam;
        std::string m_cipherlist;
        int m_min_version;
        int m_max_version;
        std::set<std::unique_ptr<PKIXIdentity>> m_identities;
        SSL_CTX * m_ssl_ctx = nullptr;
        // spdlog::logger m_log;
    };

    class pkix_error : public std::runtime_error {
        using std::runtime_error::runtime_error;
    };

    class pkix_config_error : public pkix_error {
        using pkix_error::pkix_error;
    };

    class pkix_identity_load_error : public pkix_config_error {
        using pkix_config_error::pkix_config_error;
    };

    class dhparam_error : public pkix_error {
        using pkix_error::pkix_error;
    };
}

#endif //METRE_PKIX_H
