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
#include <boost/scope_exit.hpp>
#include <http/error.hpp>
#include <ios>
#include <zlib.h>
#include <fmt/format.h>
#include <boost/iostreams/device/back_inserter.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filter/gzip.hpp>

static const std::string crlf = "\r\n";
static auto crlf_begin = crlf.begin();
static auto crlf_end = crlf.end();

static const char header_kv_delim = ':';
static const std::string http11 = "HTTP/1.1";

std::string http::session::recv_line() {
    std::ostringstream line;
    auto line_end = std::search(data_begin, data_end, crlf_begin, crlf_end);
    // While we have not encountered a crlf in our buffer
    while(line_end == data_end) {
        // Add what we have so far to our line
        std::copy(data_begin, data_end, std::ostream_iterator<char>(line));
        // Read more data
        data_begin = begin(buffer);
        int n = ::recv(sockfd, data_begin, buffer_size, 0);
        if (n == 0) throw http::premature_close("Socket closed while receiving line data");
        http::check_error(n);
        BOOST_LOG_TRIVIAL(info) << "        recv'd " << n << " from fd #" << sockfd;
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
    ssize_t unsent = size;
    while(unsent > 0) {
        ssize_t sent = ::send(sockfd, start, unsent, 0);
        BOOST_LOG_TRIVIAL(info) << "        sent " << sent << " from fd #" << sockfd;
        http::check_error(sent);
        unsent -= sent;
    }
    BOOST_LOG_TRIVIAL(info) << "        fd #" << sockfd << " finished sending " << size;
}

http::request http::session::recv_request() {
    const std::string reqln = recv_line();
    auto reqln_end = end(reqln);
    auto method_start = begin(reqln);
    auto method_end = std::find(method_start, reqln_end, ' ');
    std::string method = {method_start, method_end};
    if(method_end == reqln_end) throw http::response{400};

    auto req_uri_start = method_end + 1;
    auto req_uri_end = std::find(req_uri_start, reqln_end, ' ');
    if(req_uri_end == reqln_end) throw http::response{400};

    auto version_start = req_uri_end + 1;
    auto version_end = reqln_end;
    if(!std::equal(version_start, version_end, begin(http11), end(http11))) {
        throw http::response{505};
    }

    std::unordered_map<std::string, std::string> headers;
    for(std::string headerline = recv_line(); !headerline.empty(); headerline = recv_line()) {
        auto key_start = headerline.begin();
        auto key_end = std::find(headerline.begin(), headerline.end(), header_kv_delim);
        auto value_start = key_end + 1;
        auto value_end = headerline.end();
        headers.emplace(std::piecewise_construct,
                        std::forward_as_tuple(key_start, key_end),
                        std::forward_as_tuple(value_start, value_end));
    }
    
    return {
        {method_start, method_end},
        {req_uri_start, req_uri_end},
        {version_start, version_end},
        headers
    };
}

void http::session::transfer_id(http::response r) {
    r.headers["Content-Length"] = fmt::format("{}", r.body.size());
    std::ostringstream sstr;
    sstr << "HTTP/1.1 " << r.code << " " << r.reason << "\r\n";
    for(auto [header, val] : r.headers) {
        sstr << header << ": " << val << "\r\n";
    }
    sstr << "\r\n";
    sstr << r.body;
    std::string bytes = sstr.str();
    send_all(
        reinterpret_cast<const unsigned char*>(bytes.data()),
        static_cast<ssize_t>(bytes.size())
    );
}

void http::session::transfer_chunked(http::response r, std::size_t chunk_size) {
    r.headers["Transfer-Encoding"] = "chunked";
    std::ostringstream sstr;
    sstr << "HTTP/1.1 " << r.code << " " << r.reason << "\r\n";
    for(auto [header, val] : r.headers) {
        sstr << header << ": " << val << "\r\n";
    }
    sstr << "\r\n";
    sstr << std::hex;
    const int total_chunks = r.body.size() / chunk_size + (r.body.size() % chunk_size != 0);
    const char* begin = r.body.data();
    for(int i = 0; i < total_chunks; i++) {
        std::size_t this_chunk_size;
        if(r.body.size() % chunk_size == 0 || i != total_chunks - 1) {
            this_chunk_size = chunk_size;
        } else {
            this_chunk_size = r.body.size() % chunk_size;
        }
        sstr << this_chunk_size << "\r\n";
        sstr << std::string_view{begin, this_chunk_size} << "\r\n";
        begin += chunk_size;
    }
    BOOST_LOG_TRIVIAL(info) << "Chunked " << (begin - r.body.data()) << " bytes";
    sstr << "0\r\n";
    std::string bytes = sstr.str();
    //BOOST_LOG_TRIVIAL(info) << bytes << "!!!!!!";
    send_all(
        reinterpret_cast<const unsigned char*>(bytes.data()),
        static_cast<ssize_t>(bytes.size())
    );
}

http::response http::session::encode_id(http::response x) {
    return x;
}

http::response http::session::encode_gzip(http::response x) {
    namespace io = boost::iostreams;
    http::response encoded = x;
    encoded.headers["Content-Encoding"] = "gzip";
    encoded.body.clear();
    {
        io::filtering_ostream out;
        out.push(io::gzip_compressor{});
        out.push(io::back_inserter(encoded.body));
        out << x.body;
    }
    return encoded;
}

void http::session::send_response(http::response response) {
    http::response encoded;
    if(current_request.headers["Accept-Encoding"].find("gzip") != std::string::npos) {
        encoded = encode_gzip(response);
    } else {
        encoded = encode_id(response);
    }
    //transfer_chunked(encoded);
    transfer_id(encoded);
    BOOST_LOG_TRIVIAL(info) << "        fd #" << sockfd << " finished sending this response";
}

void http::session::operator()(int fd) {
    sockfd = fd;
    BOOST_SCOPE_EXIT(&sockfd) {
        ::shutdown(sockfd, SHUT_RDWR);
        ::close(sockfd);
        BOOST_LOG_TRIVIAL(info) << "        closed fd #" << sockfd;
    } BOOST_SCOPE_EXIT_END
    data_begin = buffer.begin();
    data_end = data_begin;
    BOOST_LOG_TRIVIAL(info) << "Handling new client";
    try {
        bool keep_alive = true;
        do {
            current_request = recv_request();
            http::request& req = current_request;
            keep_alive = current_request.headers["Connection"] != "close";
            BOOST_LOG_TRIVIAL(info) << req.method << " " << req.uri << " " << req.version;
            for(auto pair : req.headers) {
                BOOST_LOG_TRIVIAL(info) << "    " << pair.first << ":" << pair.second;
            }
            handle_request(req);
            BOOST_LOG_TRIVIAL(info) << "---------------------------";
        } while (keep_alive);
    } catch (const http::response& err) {
        send_response(err);
    } catch (const http::premature_close& err) {
        BOOST_LOG_TRIVIAL(error) << "Client stopped sending prematurely";
    } catch (const std::system_error& err) {
        if(err.code().value() == EAGAIN) {
            BOOST_LOG_TRIVIAL(error) << "Read or write timed out";
        } else {
            BOOST_LOG_TRIVIAL(error) << "System error " << err.code() << ": " << err.what();
        }
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

void http::session::handle_request(http::request req) {
    auto requested_path = fs::path{req.uri}.lexically_normal();
    try {        
        auto mapped_path = chroot_map(requested_path, "www");
        BOOST_LOG_TRIVIAL(info) << "* Mapping request to " << *mapped_path;
        if(mapped_path) {
            std::fstream input{*mapped_path};
            if(input.is_open()) {
                send_response(http::serve_file(requested_path, input));
            } else if (fs::is_directory(*mapped_path)) {
                send_response(http::serve_index(requested_path, *mapped_path));
            } else {
                send_response(http::serve_404(requested_path));
            }
        } else {
            send_response({
                403, "Forbidden",
                {{"Content-Type", "text/plain; charset=utf-8"}},
                "403 Forbidden"
            });
        }
    } catch (const fs::filesystem_error& err) {
        if(err.code() == boost::system::errc::no_such_file_or_directory) {
            BOOST_LOG_TRIVIAL(info) << "* No such file or directory (404)";
            send_response(http::serve_404(requested_path));
        } else {
            throw;
        }
    }
}
