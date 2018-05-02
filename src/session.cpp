#include <http/session.hpp>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sstream>
#include <optional>
#include <string>
#include <algorithm>
#include <iterator>
#include <boost/log/trivial.hpp>
#include <system_error>
#include <csignal>

void errmaybe(int);

static const std::string crlf = "\r\n";
using std::begin;
using std::end;

using namespace std::literals::string_literals;
static const char header_kv_delim = ':';
static const auto http11 = "HTTP/1.1"s;
std::array implemented_methods = {
    "OPTIONS"s, "GET"s, "HEAD"s, "POST"s, "PUT"s, "DELETE"s, "TRACE"s, "CONNECT"s
};

std::optional<std::string> http::session::recv_line() {
    std::ostringstream line;
    auto line_end = std::search(data_begin, data_end, begin(crlf), end(crlf));
    // While we have not encountered a crlf in our buffer
    while(line_end == data_end) {
        // Add what we have so far to our line
        std::copy(data_begin, data_end, std::ostream_iterator<char>(line));
        // Read more data
        data_begin = begin(buffer);
        int n = ::recv(sockfd, data_begin, buffer_size, 0);
        errmaybe(n);
        if(n == 0) {
            return std::nullopt;
        }
        data_end = data_begin + n;
        // Search for newline again
        line_end = std::search(data_begin, data_end, begin(crlf), end(crlf));
    }
    //We've found a line, copy it
    std::copy(data_begin, line_end, std::ostream_iterator<char>(line));
    data_begin = line_end + crlf.size();
    return line.str();
}

void http::session::send_all(const unsigned char* start, ssize_t size) {
    while(size > 0) {
        ssize_t sent = ::send(sockfd, start, size, 0);
        errmaybe(sent);
        size -= sent;
    }
}

std::optional<http::request> http::session::recv_request() {
    std::optional<std::string> maybe_reqln = recv_line();
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
    for(std::string headerline = recv_line().value(); !headerline.empty(); headerline = recv_line().value()) {
        auto key_start = headerline.begin();
        auto key_end = std::find(headerline.begin(), headerline.end(), header_kv_delim);
        auto value_start = key_end + 1;
        auto value_end = headerline.end();
        headers.emplace(std::piecewise_construct,
                        std::forward_as_tuple(key_start, key_end),
                        std::forward_as_tuple(value_start, value_end));
    }
    
    return {{
        {method_start, method_end},
        {req_uri_start, req_uri_end},
        {version_start, version_end},
        headers
    }};
}

void http::session::send_response(http::response r) {
    std::ostringstream sstr;
    sstr << "HTTP/1.1 " << r.code << "[ReasonHere]\r\n";
    sstr << "Content-Length:" << r.body.size() << "\r\n";
    sstr << "\r\n";
    sstr << r.body;
    std::string bytes = sstr.str();
    send_all(
        reinterpret_cast<const unsigned char*>(bytes.data()),
        static_cast<ssize_t>(bytes.size())
    );
}

http::response handle_request(http::request req);
void http::session::operator()(int fd) {
    using std::begin;
    using std::end;
    // "constructor"
    sockfd = fd;
    data_begin = begin(buffer);
    data_end = data_begin;
    // end "constructor"

    BOOST_LOG_TRIVIAL(info) << "Handling new client";
    try {
        bool keep_alive = true;
        do {
            std::optional<http::request> maybe_req = recv_request();
            if(!maybe_req) {
                BOOST_LOG_TRIVIAL(error) << "Failed to read request from new connection";
                return;
            }
            http::request& req = *maybe_req;
            keep_alive = req.headers["Connection"] != "close";
            // Now consider
            // * Should we keep-alive the connection?
            //     Yes, unless Connection: close
            // * What is the transfer-encoding?
            //     We should support chunked
            // * What is the content-encoding?
            //     We should support gzip
            // * Which language should be sent?
            // These questions make up the "content negotiation algorithm"

            // For debug purposes:
            BOOST_LOG_TRIVIAL(info) << req.method << " " << req.uri << " " << req.version   ;
            for(auto pair : req.headers) {
                BOOST_LOG_TRIVIAL(info) << pair.first << ":" << pair.second;
            }
            
            http::response res = handle_request(req);
            BOOST_LOG_TRIVIAL(info) << "-------";
            send_response(res);
        } while (keep_alive);
    } catch (const http::response& err) {
        send_response(err);
        BOOST_LOG_TRIVIAL(error) << "Something went wrong: " << err.code;
    } catch (const std::system_error& err) {
        BOOST_LOG_TRIVIAL(error) << "System error " << err.code() << ": " << err.what();
    } catch (...) {
        BOOST_LOG_TRIVIAL(error) << "Something went badly wrong.";
    }
}

#include <fstream>
#include <sstream>
http::response handle_request(http::request req) {
    std::stringstream uri_strm;
    uri_strm << "srv" << req.uri;
    std::fstream input{uri_strm.str()};
    if(input.is_open()) {
        return {200, {
            std::istreambuf_iterator<char>{input},
            std::istreambuf_iterator<char>{}
        }};
    } else {
        return {404, "File not found :("};
    }
}