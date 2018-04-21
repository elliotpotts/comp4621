#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <system_error>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <sstream>
#include <iterator>
#include <array>
#include <memory>
#include <atomic>
#include <signal.h>

const short http_port = 9999;//80;
const int backlog = 128; // arbitrary
const std::string crlf = "\r\n";
const int buffer_size = 2048; // arbitraty

void errmaybe(int return_val) {
    if(return_val < 0 && errno > 0) {
        throw std::system_error(errno, std::generic_category());
    }
}

using byte_buf = std::array<char, buffer_size>;

// Line-buffered tcp socket
class tcp_socket {
    int fd;
    byte_buf buffer;
    byte_buf::iterator data_begin;
    byte_buf::iterator data_end;
    public:
    tcp_socket(int connected) : fd(connected), data_begin(begin(buffer)), data_end(begin(buffer)) {
    };
    // Received data until a crlf is found. Return the data excluding the crlf
    std::string recvline() {
        std::ostringstream line;
        auto line_end = std::search(data_begin, data_end, begin(crlf), end(crlf));
        // While we have not encountered a crlf in our buffer
        while(line_end == data_end) {
            // Add what we have so far to our line
            std::copy(data_begin, data_end, std::ostream_iterator<char>(line));
            // Read more data
            data_begin = begin(buffer);
            int n = ::recv(fd, data_begin, buffer_size, 0);
            errmaybe(n);
            data_end = data_begin + n + 1;
            // Search for newline again
            line_end = std::search(data_begin, data_end, begin(crlf), end(crlf));
        }
        //We've found a line, copy it
        std::copy(data_begin, line_end, std::ostream_iterator<char>(line));
        data_begin = line_end + crlf.size();
        return line.str();
    }
    ~tcp_socket() {
        ::close(fd);
    }
};

/*namespace method {
    #define decl_method(m) struct m { std::string str = #m; }; 
    decl_method(OPTIONS)
    decl_method(GET)
}*/

using namespace std::literals::string_literals;
std::array allowed_methods = {
    "OPTIONS"s, "GET"s, "HEAD"s, "POST"s, "PUT"s, "DELETE"s, "TRACE"s, "CONNECT"s
};

std::string supported_version = "HTTP/1.1";

struct http_request {
    std::string method;
    std::string uri;
    byte_buf body;
};

struct http_response {
    int code;
    static http_response bad_request() { return {400}; }
    static http_response version_not_supported() { return {505}; }
};

// Receive a http request from a client via tcp socket
http_request recv_request(tcp_socket& sock) {
    std::string reqln = sock.recvline();
    auto reqln_end = end(reqln);

    auto method_start = begin(reqln);
    auto method_end = std::find(method_start, reqln_end, ' ');
    if(method_end == reqln_end) throw http_response::bad_request();

    auto req_uri_start = method_end + 1;
    auto req_uri_end = std::find(req_uri_start, reqln_end, ' ');
    if(req_uri_end == reqln_end) throw http_response::bad_request();

    auto version_start = req_uri_end + 1;
    auto version_end = reqln_end;
    if(!std::equal(version_start, version_end, begin(supported_version), end(supported_version))) throw http_response::version_not_supported();

    return {
        {method_start, method_end},
        {req_uri_start, req_uri_end}
    };
}

class http_server {
    int fd;
    void handle_client(tcp_socket&& client) {
        try {
            auto req = recv_request(client);
            std::cout << "[" << req.method << "] [" << req.uri << "]" << std::endl;
        } catch (const http_response& err) {
            std::cout << "Something went wrong: " << err.code << std::endl;
        }
    }
    public:
    http_server() : fd(::socket(AF_INET, SOCK_STREAM, 0)) {
        errmaybe(fd);
        int enable_reuse = 1;
        errmaybe(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enable_reuse, sizeof(enable_reuse)));
        const sockaddr_in listen_address = {
            AF_INET,           // sin_family
            htons(http_port),  // sin_port
            INADDR_ANY         // sin_addr
        };
        errmaybe(::bind(fd, reinterpret_cast<const sockaddr*>(&listen_address), sizeof(listen_address)));
        errmaybe(::listen(fd, backlog));
    }
    ~http_server() {
        ::close(fd);
    }
    void serve() {
        while(true) {
            sockaddr_in client_addr;
            socklen_t addrlen = sizeof(client_addr);
            int clientfd = ::accept(fd, reinterpret_cast<sockaddr*>(&client_addr), &addrlen);
            errmaybe(clientfd);
            handle_client({clientfd});
        }
    }
};

void handle_signal(int) {
}

int main() {
    // Install signal handler for SIGINT aka keyboard interrupt
    struct sigaction sa;
    sa.sa_handler = handle_signal;
    //sa.sa_mask = 0;
    sa.sa_flags = 0;
    sa.sa_restorer = nullptr;
    errmaybe(sigaction(SIGINT, &sa, nullptr));
    
    // Start server
    std::cout << "Listening...\n";
    try {
        http_server s;
        s.serve();
    } catch (const std::system_error& ex) {
        if (ex.code().value() == EINTR) {
            std::cout << "Keyboard interrupt. Stopping server" << std::endl;
        } else {
            throw;
        }
    }
}