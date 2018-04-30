#include <http/request.hpp>
#include <http/response.hpp>
#include <http/socket.hpp>
#include <algorithm>
#include <string>
#include <sstream>
#include <vector>

std::vector<std::string> split(std::string s, char delim) {
    std::vector<std::string> parts;
    auto it = s.begin();
    auto end = s.end();
    while(it != end) {
        auto delim_it = std::find(it, end, delim);
        parts.emplace_back(it, delim_it);
        it = delim_it;
    }
    return parts;
}

template<typename Cont, typename T>
bool contains(const Cont& xs, T x) {
    using std::begin;
    using std::end;
    auto end_it = end(xs);
    return std::find(begin(xs), end_it, x) != end_it;
}

using namespace std::literals::string_literals;
static const char header_kv_delim = ':';
static const auto http11 = "HTTP/1.1"s;
std::array implemented_methods = {
    "OPTIONS"s, "GET"s, "HEAD"s, "POST"s, "PUT"s, "DELETE"s, "TRACE"s, "CONNECT"s
};

std::optional<http::request> http::recv_request(http::socket& sock) {
    std::optional<std::string> maybe_reqln = sock.recvline();
    if(!maybe_reqln) {
        return std::nullopt;
    }
    const std::string& reqln = *maybe_reqln;
    auto reqln_end = end(reqln);
    auto method_start = begin(reqln);
    auto method_end = std::find(method_start, reqln_end, ' ');
    std::string method = {method_start, method_end};
    if(method_end == reqln_end) throw http::response{404};


    auto req_uri_start = method_end + 1;
    auto req_uri_end = std::find(req_uri_start, reqln_end, ' ');
    if(req_uri_end == reqln_end) throw http::response{404};

    auto version_start = req_uri_end + 1;
    auto version_end = reqln_end;
    if(!std::equal(version_start, version_end, begin(http11), end(http11))) {
        throw http::response{501};
    }

    std::unordered_map<std::string, std::string> headers;
    for(std::string headerline = sock.recvline().value(); !headerline.empty(); headerline = sock.recvline().value()) {
        auto key_start = headerline.begin();
        auto key_end = std::find(headerline.begin(), headerline.end(), header_kv_delim);
        auto value_start = key_end + 1;
        auto value_end = headerline.end();
        headers.emplace(std::piecewise_construct,
                        std::forward_as_tuple(key_start, key_end),
                        std::forward_as_tuple(value_start, value_end));
    }

    std::ostringstream body;
    auto content_length_it = headers.find("Content-Length");
    (void) content_length_it;
    return {{
        {method_start, method_end},
        {req_uri_start, req_uri_end},
        {version_start, version_end},
        headers
    }};
}