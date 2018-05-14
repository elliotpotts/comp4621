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
#include <csignal>
#include <http/index.hpp>
#include <boost/system/error_code.hpp>

void errmaybe(int);

static const std::string crlf = "\r\n";
using std::begin;
using std::end;

using namespace std::literals::string_literals;
static const char header_kv_delim = ':';
static const auto http11 = "HTTP/1.1"s;

// Receives a line from the client, or returns empty if 
// the socket has been orderly shutdown.
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
        BOOST_LOG_TRIVIAL(info) << "        recv'd " << n << " from fd #" << sockfd;
        errmaybe(n); //TODO: deal with 104 (connection reset by peer)
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
        BOOST_LOG_TRIVIAL(info) << "        sent " << sent << " from fd #" << sockfd;
        errmaybe(sent);
        size -= sent;
    }
    BOOST_LOG_TRIVIAL(info) << "        fd #" << sockfd << " finished sending this chunk";
}

// Receives a request from the client, or returns empty if 
// the socket has been orderly shutdown.
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
    sstr << "HTTP/1.1 " << r.code << " " << r.reason << "\r\n";
    sstr << "Content-Type: " << r.content_type << "\r\n";
    sstr << "Content-Length:" << r.body.size() << "\r\n";
    sstr << "\r\n";
    sstr << r.body;
    std::string bytes = sstr.str();
    send_all(
        reinterpret_cast<const unsigned char*>(bytes.data()),
        static_cast<ssize_t>(bytes.size())
    );
    BOOST_LOG_TRIVIAL(info) << "        fd #" << sockfd << " finished sending this response";
}

http::response handle_request(http::request req);
// HTTP "Main Loop"
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
                ::shutdown(sockfd, SHUT_RDWR);
                ::close(sockfd);
                BOOST_LOG_TRIVIAL(info) << "        closed fd #" << sockfd;
                return;
            }
            http::request& req = *maybe_req;
            keep_alive = req.headers["Connection"] != "close";
            BOOST_LOG_TRIVIAL(info) << req.method << " " << req.uri << " " << req.version   ;
            for(auto pair : req.headers) {
                BOOST_LOG_TRIVIAL(info) << "    " << pair.first << ":" << pair.second;
            }
            http::response res = handle_request(req);
            BOOST_LOG_TRIVIAL(info) << "---------------------------";
            send_response(res);
        } while (keep_alive);
    } catch (const http::response& err) {
        send_response(err);
        BOOST_LOG_TRIVIAL(error) << "Something went wrong: " << err.code;
    } catch (const std::system_error& err) {
        BOOST_LOG_TRIVIAL(error) << "System error " << err.code() << ": " << err.what();
    } catch (const std::exception& err) {
        BOOST_LOG_TRIVIAL(error) << "Something went badly wrong: " << err.what();
    } catch (...) {
        BOOST_LOG_TRIVIAL(error) << "Something went extremely wrong";
    }
}

namespace fs = boost::filesystem;

std::optional<fs::path> chroot_map(fs::path original, fs::path root) {
    fs::path mapped = root;                    // "./rel_root"
    mapped /= original;                        // "./rel_root/../../etc/passwd"
    fs::path canon = fs::canonical(mapped);    // "/etc/passwd"
    fs::path ceiling = fs::canonical(root);    // "/www/rel_root"
    auto [first_mismatch, _] = std::mismatch (
        ceiling.begin(), ceiling.end(),
        canon.begin(), canon.end()
    );
    if(first_mismatch == ceiling.end()) {
        return mapped;
    } else {
        return std::nullopt;
    }
}

http::response handle_request(http::request req) {
    auto requested_path = fs::path{req.uri}.lexically_normal();
    try {        
        auto mapped_path = chroot_map(requested_path, "www");
        BOOST_LOG_TRIVIAL(info) << "* Mapping request to " << *mapped_path;
        if(mapped_path) {
            std::fstream input{*mapped_path};
            if(input.is_open()) {
                return http::serve_file(requested_path, input);
            } else if (fs::is_directory(*mapped_path)) {
                return http::serve_index(requested_path, *mapped_path);
            } else {
                return http::serve_404(requested_path);
            }
        } else {
            return {403, "Forbidden", "text/plain; charset=utf-8", "403 Forbidden"};
        }
    } catch (const fs::filesystem_error& err) {
        if(err.code() == boost::system::errc::no_such_file_or_directory) {
            BOOST_LOG_TRIVIAL(info) << "* No such file or directory (404)";
            return http::serve_404(requested_path);
        } else {
            throw;
        }
    }
}
