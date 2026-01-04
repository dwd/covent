//
// Created by dwd on 1/3/25.
//

#include <covent/http.h>
#include <algorithm>
#include <ranges>
#include <fmt/format.h>

using namespace covent::http;

Message::Message(URI const & u) : uri(u), request(true) {
    header["host"] = uri->netloc;
}
Message::Message(std::string_view u) : uri(u), request(true) {
    header["host"] = uri->netloc;
}


unsigned long Message::process_status(std::string_view &data) {
    unsigned long int read = 0;
    // http_version SP status_code SP status_reason CR LF
    auto crlf = data.find("\r\n");
    if (crlf == std::string_view::npos) {
        return 0;
    }
    auto ver_space = data.find(' ');
    if (ver_space == std::string_view::npos) {
        // TODO : ERROR
        status_code = 0;
        return data.length();
    }
    // Just trim off the HTTP version
    auto space = data.find(' ', ver_space + 1);
    std::string status_string{data.substr(ver_space + 1, space - ver_space - 1)};
    status_code = std::stoi(status_string);
    status_text = data.substr(space + 1, crlf - space - 1);
    read = crlf + 2;
    data.remove_prefix(read);
    return read;
}

unsigned long Message::process_header(std::string_view &data) {
    unsigned long read = 0;
    // Parse out headers. We ignore folding.
    auto crlf = data.find("\r\n");
    if (crlf == std::string_view::npos) {
        return read;
    }
    if (crlf == 0) {
        read += 2;
        data.remove_prefix(2);
        header_read = true;
        return read;
    }
    auto colon = data.find_first_of(":\r\n");
    if (colon == std::string_view::npos || colon == crlf) {
        // TODO : ERROR
        return read + data.length();
    }
    std::string field_name{data.substr(0, colon)};
    std::ranges::transform(field_name, field_name.begin(), [](auto c) { return std::tolower(c); });
    auto ows = data.find_first_not_of(" \t", colon + 1);
    std::string field_value{data.substr(ows, crlf - ows)};
    if (auto end_ows = field_value.find_last_not_of(" \t"); end_ows != std::string::npos) {
        field_value = field_value.substr(0, end_ows + 1);
    }
    header[field_name] = field_value;
    data.remove_prefix(crlf + 2);
    read += crlf + 2;
    return read;
}

void Message::check_body_type() {
    if (!body_remaining.has_value() && !chunked) {
        if (header.contains("transfer-encoding")) {
            if (header["transfer-encoding"].contains("chunked")) {
                // TODO !
                chunked = true;
            }
        } else {
            if (header.contains("content-length")) {
                body_remaining = std::stoi(header["content-length"]);
            } else {
                body_remaining = 0;
            }
        }
    }
}

unsigned long Message::read_body(std::string_view & data) {
    unsigned long read = 0;
    if (body_remaining.value() > 0) {
        body += data;
        body_remaining = body_remaining.value() - data.length();
        read += data.length();
        data.remove_prefix(data.length());
    }
    if (body_remaining.value() == 0) {
        complete = true;
    }
    return read;
}

std::tuple<unsigned long, unsigned long> Message::read_body_chunk_header(std::string_view & data) {
    unsigned long chunk = 0;
    auto crlf = data.find("\r\n");
    if (crlf == std::string_view::npos) {
        return {0, 0};
    }
    auto colon = data.find(';');
    if (colon == std::string_view::npos || colon > crlf) {
        colon = crlf;
    }
    std::string hex{data.substr(0, colon)};
    auto chunk_len = std::stol(hex, nullptr, 16);
    chunk += crlf + 2;
    data.remove_prefix(crlf + 2);
    return {chunk, chunk_len};
}

unsigned long Message::read_body_chunk_normal(std::string_view & data, unsigned long chunk_len) {
    if (data.length() >= (chunk_len + 2)) {
        unsigned int chunk = 0;
        // CRLF
        body += data.substr(0, chunk_len);
        chunk += chunk_len;
        data.remove_prefix(chunk_len);
        if (data.starts_with("\r\n")) {
            data.remove_prefix(2);
            chunk += 2;
        }
        return chunk;
    } else {
        return 0;
    }
}

unsigned long Message::read_body_chunk_final(std::string_view &data) {
    unsigned long chunk = 0;
    header_read = false;
    while (!header_read) {
        auto status = process_header(data);
        chunk += status;
        if (status == 0) {
            header_read = true;
            return 0;
        }
    }
    return chunk;
}

unsigned long Message::read_body_chunked(std::string_view & data) {
    unsigned long read = 0;
    while (!complete) {
        auto [chunk, chunk_len] = read_body_chunk_header(data);
        if (chunk == 0) {
            return read;
        }
        if (chunk_len == 0) {
            auto status = read_body_chunk_final(data);
            chunk += status;
            if (status == 0) {
                return read;
            } else {
                complete = true;
            }
        } else {
            auto status  = read_body_chunk_normal(data, chunk_len);
            chunk += status;
            if (status == 0) {
                return read;
            }
        }
        read += chunk;
    }
    return read;
}

unsigned long Message::process(std::string_view data) {
    unsigned long int read = 0;
    if (status_code == -1) {
        auto status = process_status(data);
        read += status;
    }
    if (status_code == -1) {
        return read;
    }
    while (!header_read) {
        auto status = process_header(data);
        read += status;
        if (status == 0) {
            return read;
        }
    }
    if (header_read) {
        check_body_type();
        if (body_remaining.has_value()) {
            read += read_body(data);
        } else if (chunked) {
            read += read_body_chunked(data);
        }
    }
    return read;
}

std::string Message::render_request(Method method) const {
    if (!request) throw std::logic_error("Not a request");
    const char * mstr = nullptr;
    switch (method) {
        case Method::GET:
            mstr = "GET";
            break;
        case Method::POST:
            mstr = "POST";
            break;
        case Method::DELETE:
            mstr = "DELETE";
            break;
        case Method::PUT:
            mstr = "PUT";
            break;
    }
    return fmt::format("{} {} HTTP/1.1\r\n", mstr, uri->path);
}

std::string Message::render_header() const {
    std::string h;
    for (auto const & [field, value] : header) {
        h += fmt::format("{}: {}\r\n", field, value);
    }
    return h + "\r\n";
}

std::string Message::render_body() const {
    return body;
}

