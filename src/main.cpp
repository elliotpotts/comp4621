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

const short http_port = 9999;//80;
const int backlog = 128; // arbitrary
const std::string crlf = "\r\n";
const int buffer_size = 2048; // arbitraty

void errmaybe() {
    if(errno > 0) {
        throw std::system_error(errno, std::system_category());
    }
}

using byte_buf = std::array<char, buffer_size>;

class tcp_socket {
    int fd;
    byte_buf buffer;
    byte_buf::iterator data_begin;
    byte_buf::iterator data_end;
    public:
    tcp_socket(int connected) : fd(connected), data_begin(begin(buffer)), data_end(begin(buffer) + 1) {
    };
    std::string recvline() {
        std::ostringstream line;
        auto line_end = std::search(data_begin, data_end, begin(crlf), end(crlf));
        // While we have not encountered a crlf in our buffer
        while(line_end == data_end) {
            // Add what we have so far to our line
            std::copy(data_begin, data_end, std::ostream_iterator<char>(line));
            data_begin = begin(buffer);
            // Read more data 
            int n = ::recv(fd, data_begin, buffer_size, 0);
            errmaybe();
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
        errmaybe();
    }
};

class http_server {
    int fd;
    void handle_client(tcp_socket&& client) {
        std::cout << "Got new client\n";
        std::cout << client.recvline() << std::endl;
        std::cout << client.recvline() << std::endl;
        std::cout << client.recvline() << std::endl;
    }
    public:
    http_server() : fd(::socket(AF_INET, SOCK_STREAM, 0)) {
        errmaybe();
        const sockaddr_in listen_address = {
            AF_INET,           // sin_family
            htons(http_port),  // sin_port
            INADDR_ANY         // sin_addr
        };
        ::bind(fd, reinterpret_cast<const sockaddr*>(&listen_address), sizeof(listen_address));
        errmaybe();
        ::listen(fd, backlog);
        errmaybe();
    }
    ~http_server() {
        close(fd);
    }
    
    void serve() {
        while(true) {
            sockaddr_in client_addr;
            socklen_t addrlen = sizeof(client_addr);
            int clientfd = ::accept(fd, reinterpret_cast<sockaddr*>(&client_addr), &addrlen);
            errmaybe();
            handle_client({clientfd});
        }
    }
    void shutdown();
};

int main() {
    std::cout << "Hello, World!\n";
    http_server s;
    s.serve();
}