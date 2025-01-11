//
// Created by dwd on 1/3/25.
//
#include <covent/http.h>

using namespace covent::http;

URI::URI(std::string_view text) {
    parse(text);
}

    void URI::parse(std::string_view text) {
        auto colon = text.find(':');
        if (colon == std::string_view::npos) {
            throw std::runtime_error("No scheme colon");
        }
        scheme = text.substr(0, colon);
        if (scheme == "http") {
            port = 80;
        } else if (scheme == "https") {
            port = 443;
        }
        text.remove_prefix(colon);
        auto host_start = text.find_first_not_of(":/");
        text.remove_prefix(host_start);
        auto host_end = text.find_first_of(":/");
        if (host_end == std::string_view::npos) {
            host = text;
            netloc = host;
            path = "/";
            return;
        }
        host = text.substr(0, host_end);
        if (text[host_end] == ':') {
            text.remove_prefix(host_end + 1);
            auto slash = text.find('/');
            if (slash == std::string_view::npos) {
                std::string portstr{text};
                netloc = host + ':' + portstr;
                port = std::stoi(portstr);
                path = "/";
                return;
            }
            std::string portstr{text.substr(0, slash)};
            netloc = host + ':' + portstr;
            port = std::stoi(portstr);
            text.remove_prefix(slash);
        } else {
            netloc = host;
            text.remove_prefix(host_end);
        }
        path = text;
    }


